// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui.h"

#include <set>
#include <vector>

#include "net/base/file_stream.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/extensions/extension_bookmark_manager_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/bindings_policy.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/page_transition_types.h"
#include "chrome/common/url_constants.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

namespace {

// De-dupes the items in |list|. Assumes the values are strings.
void CleanUpDuplicates(ListValue* list) {
  std::set<std::string> seen_values;

  // Loop backwards as we may be removing items.
  for (size_t i = list->GetSize() - 1; (i + 1) > 0; --i) {
    std::string value;
    if (!list->GetString(i, &value)) {
      NOTREACHED();
      continue;
    }

    if (seen_values.find(value) == seen_values.end())
      seen_values.insert(value);
    else
      list->Remove(i, NULL);
  }
}

// Helper class that is used to track the loading of the favicon of an
// extension.
class ExtensionWebUIImageLoadingTracker : public ImageLoadingTracker::Observer {
 public:
  ExtensionWebUIImageLoadingTracker(Profile* profile,
                                    FaviconService::GetFaviconRequest* request,
                                    const GURL& page_url)
      : ALLOW_THIS_IN_INITIALIZER_LIST(tracker_(this)),
        request_(request),
        extension_(NULL) {
    // Even when the extensions service is enabled by default, it's still
    // disabled in incognito mode.
    ExtensionService* service = profile->GetExtensionService();
    if (service)
      extension_ = service->GetExtensionByURL(page_url);
  }

  void Init() {
    if (extension_) {
      ExtensionResource icon_resource =
          extension_->GetIconResource(Extension::EXTENSION_ICON_BITTY,
                                      ExtensionIconSet::MATCH_EXACTLY);

      tracker_.LoadImage(extension_, icon_resource,
                         gfx::Size(kFavIconSize, kFavIconSize),
                         ImageLoadingTracker::DONT_CACHE);
    } else {
      ForwardResult(NULL);
    }
  }

  virtual void OnImageLoaded(SkBitmap* image, const ExtensionResource& resource,
                             int index) {
    if (image) {
      std::vector<unsigned char> image_data;
      if (!gfx::PNGCodec::EncodeBGRASkBitmap(*image, false, &image_data)) {
        NOTREACHED() << "Could not encode extension favicon";
      }
      ForwardResult(RefCountedBytes::TakeVector(&image_data));
    } else {
      ForwardResult(NULL);
    }
  }

 private:
  ~ExtensionWebUIImageLoadingTracker() {}

  // Forwards the result on the request. If no favicon was available then
  // |icon_data| may be backed by NULL. Once the result has been forwarded the
  // instance is deleted.
  void ForwardResult(scoped_refptr<RefCountedMemory> icon_data) {
    bool know_icon = icon_data.get() != NULL && icon_data->size() > 0;
    request_->ForwardResultAsync(
        FaviconService::FaviconDataCallback::TupleType(request_->handle(),
            know_icon, icon_data, false, GURL()));
    delete this;
  }

  ImageLoadingTracker tracker_;
  scoped_refptr<FaviconService::GetFaviconRequest> request_;
  const Extension* extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebUIImageLoadingTracker);
};

}  // namespace

const char ExtensionWebUI::kExtensionURLOverrides[] =
    "extensions.chrome_url_overrides";

ExtensionWebUI::ExtensionWebUI(TabContents* tab_contents, const GURL& url)
    : WebUI(tab_contents),
      url_(url) {
  ExtensionService* service = tab_contents->profile()->GetExtensionService();
  const Extension* extension = service->GetExtensionByURL(url);
  if (!extension)
    extension = service->GetExtensionByWebExtent(url);
  DCHECK(extension);
  // Only hide the url for internal pages (e.g. chrome-extension or packaged
  // component apps like bookmark manager.
  should_hide_url_ = !extension->is_hosted_app();

  bindings_ = BindingsPolicy::EXTENSION;
  // Bind externalHost to Extension WebUI loaded in Chrome Frame.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kChromeFrame))
    bindings_ |= BindingsPolicy::EXTERNAL_HOST;
  // For chrome:// overrides, some of the defaults are a little different.
  GURL effective_url = tab_contents->GetURL();
  if (effective_url.SchemeIs(chrome::kChromeUIScheme) &&
      effective_url.host() == chrome::kChromeUINewTabHost) {
    focus_location_bar_by_default_ = true;
  }
}

ExtensionWebUI::~ExtensionWebUI() {}

void ExtensionWebUI::ResetExtensionFunctionDispatcher(
    RenderViewHost* render_view_host) {
  // TODO(jcivelli): http://crbug.com/60608 we should get the URL out of the
  //                 active entry of the navigation controller.
  extension_function_dispatcher_.reset(
      ExtensionFunctionDispatcher::Create(render_view_host, this, url_));
  DCHECK(extension_function_dispatcher_.get());
}

void ExtensionWebUI::ResetExtensionBookmarkManagerEventRouter() {
  // Hack: A few things we specialize just for the bookmark manager.
  if (extension_function_dispatcher_->extension_id() ==
      extension_misc::kBookmarkManagerId) {
    extension_bookmark_manager_event_router_.reset(
        new ExtensionBookmarkManagerEventRouter(GetProfile(), tab_contents()));

    link_transition_type_ = PageTransition::AUTO_BOOKMARK;
  }
}

void ExtensionWebUI::RenderViewCreated(RenderViewHost* render_view_host) {
  ResetExtensionFunctionDispatcher(render_view_host);
  ResetExtensionBookmarkManagerEventRouter();
}

void ExtensionWebUI::RenderViewReused(RenderViewHost* render_view_host) {
  ResetExtensionFunctionDispatcher(render_view_host);
  ResetExtensionBookmarkManagerEventRouter();
}

void ExtensionWebUI::ProcessWebUIMessage(
    const ViewHostMsg_DomMessage_Params& params) {
  extension_function_dispatcher_->HandleRequest(params);
}

Browser* ExtensionWebUI::GetBrowser() {
  TabContents* contents = tab_contents();
  TabContentsIterator tab_iterator;
  for (; !tab_iterator.done(); ++tab_iterator) {
    if (contents == *tab_iterator)
      return tab_iterator.browser();
  }

  return NULL;
}

TabContents* ExtensionWebUI::associated_tab_contents() const {
  return tab_contents();
}

ExtensionBookmarkManagerEventRouter*
ExtensionWebUI::extension_bookmark_manager_event_router() {
  return extension_bookmark_manager_event_router_.get();
}

gfx::NativeWindow ExtensionWebUI::GetCustomFrameNativeWindow() {
  if (GetBrowser())
    return NULL;

  // If there was no browser associated with the function dispatcher delegate,
  // then this WebUI may be hosted in an ExternalTabContainer, and a framing
  // window will be accessible through the tab_contents.
  TabContentsDelegate* tab_contents_delegate = tab_contents()->delegate();
  if (tab_contents_delegate)
    return tab_contents_delegate->GetFrameNativeWindow();
  else
    return NULL;
}

gfx::NativeView ExtensionWebUI::GetNativeViewOfHost() {
  return tab_contents()->GetRenderWidgetHostView()->GetNativeView();
}

////////////////////////////////////////////////////////////////////////////////
// chrome:// URL overrides

// static
void ExtensionWebUI::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(kExtensionURLOverrides);
}

// static
bool ExtensionWebUI::HandleChromeURLOverride(GURL* url, Profile* profile) {
  if (!url->SchemeIs(chrome::kChromeUIScheme))
    return false;

  const DictionaryValue* overrides =
      profile->GetPrefs()->GetDictionary(kExtensionURLOverrides);
  std::string page = url->host();
  ListValue* url_list;
  if (!overrides || !overrides->GetList(page, &url_list))
    return false;

  ExtensionService* service = profile->GetExtensionService();

  size_t i = 0;
  while (i < url_list->GetSize()) {
    Value* val = NULL;
    url_list->Get(i, &val);

    // Verify that the override value is good.  If not, unregister it and find
    // the next one.
    std::string override;
    if (!val->GetAsString(&override)) {
      NOTREACHED();
      UnregisterChromeURLOverride(page, profile, val);
      continue;
    }
    GURL extension_url(override);
    if (!extension_url.is_valid()) {
      NOTREACHED();
      UnregisterChromeURLOverride(page, profile, val);
      continue;
    }

    // Verify that the extension that's being referred to actually exists.
    const Extension* extension = service->GetExtensionByURL(extension_url);
    if (!extension) {
      // This can currently happen if you use --load-extension one run, and
      // then don't use it the next.  It could also happen if an extension
      // were deleted directly from the filesystem, etc.
      LOG(WARNING) << "chrome URL override present for non-existant extension";
      UnregisterChromeURLOverride(page, profile, val);
      continue;
    }

    // We can't handle chrome-extension URLs in incognito mode unless the
    // extension uses split mode.
    bool incognito_override_allowed =
        extension->incognito_split_mode() &&
        service->IsIncognitoEnabled(extension);
    if (profile->IsOffTheRecord() && !incognito_override_allowed) {
      ++i;
      continue;
    }

    *url = extension_url;
    return true;
  }
  return false;
}

// static
void ExtensionWebUI::RegisterChromeURLOverrides(
    Profile* profile, const Extension::URLOverrideMap& overrides) {
  if (overrides.empty())
    return;

  PrefService* prefs = profile->GetPrefs();
  DictionaryValue* all_overrides =
      prefs->GetMutableDictionary(kExtensionURLOverrides);

  // For each override provided by the extension, add it to the front of
  // the override list if it's not already in the list.
  Extension::URLOverrideMap::const_iterator iter = overrides.begin();
  for (; iter != overrides.end(); ++iter) {
    const std::string& key = iter->first;
    ListValue* page_overrides;
    if (!all_overrides->GetList(key, &page_overrides)) {
      page_overrides = new ListValue();
      all_overrides->Set(key, page_overrides);
    } else {
      CleanUpDuplicates(page_overrides);

      // Verify that the override isn't already in the list.
      ListValue::iterator i = page_overrides->begin();
      for (; i != page_overrides->end(); ++i) {
        std::string override_val;
        if (!(*i)->GetAsString(&override_val)) {
          NOTREACHED();
          continue;
        }
        if (override_val == iter->second.spec())
          break;
      }
      // This value is already in the list, leave it alone.
      if (i != page_overrides->end())
        continue;
    }
    // Insert the override at the front of the list.  Last registered override
    // wins.
    page_overrides->Insert(0, new StringValue(iter->second.spec()));
  }
}

// static
void ExtensionWebUI::UnregisterAndReplaceOverride(const std::string& page,
    Profile* profile, ListValue* list, Value* override) {
  int index = list->Remove(*override);
  if (index == 0) {
    // This is the active override, so we need to find all existing
    // tabs for this override and get them to reload the original URL.
    for (TabContentsIterator iterator; !iterator.done(); ++iterator) {
      TabContents* tab = *iterator;
      if (tab->profile() != profile)
        continue;

      GURL url = tab->GetURL();
      if (!url.SchemeIs(chrome::kChromeUIScheme) || url.host() != page)
        continue;

      // Don't use Reload() since |url| isn't the same as the internal URL
      // that NavigationController has.
      tab->controller().LoadURL(url, url, PageTransition::RELOAD);
    }
  }
}

// static
void ExtensionWebUI::UnregisterChromeURLOverride(const std::string& page,
    Profile* profile, Value* override) {
  if (!override)
    return;
  PrefService* prefs = profile->GetPrefs();
  DictionaryValue* all_overrides =
      prefs->GetMutableDictionary(kExtensionURLOverrides);
  ListValue* page_overrides;
  if (!all_overrides->GetList(page, &page_overrides)) {
    // If it's being unregistered, it should already be in the list.
    NOTREACHED();
    return;
  } else {
    UnregisterAndReplaceOverride(page, profile, page_overrides, override);
  }
}

// static
void ExtensionWebUI::UnregisterChromeURLOverrides(
    Profile* profile, const Extension::URLOverrideMap& overrides) {
  if (overrides.empty())
    return;
  PrefService* prefs = profile->GetPrefs();
  DictionaryValue* all_overrides =
      prefs->GetMutableDictionary(kExtensionURLOverrides);
  Extension::URLOverrideMap::const_iterator iter = overrides.begin();
  for (; iter != overrides.end(); ++iter) {
    const std::string& page = iter->first;
    ListValue* page_overrides;
    if (!all_overrides->GetList(page, &page_overrides)) {
      // If it's being unregistered, it should already be in the list.
      NOTREACHED();
      continue;
    } else {
      StringValue override(iter->second.spec());
      UnregisterAndReplaceOverride(iter->first, profile,
                                   page_overrides, &override);
    }
  }
}

// static
void ExtensionWebUI::GetFaviconForURL(Profile* profile,
    FaviconService::GetFaviconRequest* request, const GURL& page_url) {
  // tracker deletes itself when done.
  ExtensionWebUIImageLoadingTracker* tracker =
      new ExtensionWebUIImageLoadingTracker(profile, request, page_url);
  tracker->Init();
}
