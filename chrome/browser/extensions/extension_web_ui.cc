// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui.h"

#include <set>
#include <vector>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/bookmarks/bookmark_manager_extension_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/tab_contents/tab_contents_iterator.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/extensions/extension_icon_set.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/page_transition_types.h"
#include "net/base/file_stream.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/favicon_size.h"

using content::WebContents;
using extensions::Extension;

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
      extension_ = service->extensions()->GetByID(page_url.host());
  }

  void Init() {
    if (extension_) {
      ExtensionResource icon_resource =
          extension_->GetIconResource(ExtensionIconSet::EXTENSION_ICON_BITTY,
                                      ExtensionIconSet::MATCH_EXACTLY);

      tracker_.LoadImage(extension_, icon_resource,
                         gfx::Size(gfx::kFaviconSize, gfx::kFaviconSize),
                         ImageLoadingTracker::DONT_CACHE);
    } else {
      ForwardResult(NULL);
    }
  }

  virtual void OnImageLoaded(const gfx::Image& image,
                             const std::string& extension_id,
                             int index) OVERRIDE {
    if (!image.IsEmpty()) {
      std::vector<unsigned char> image_data;
      if (!gfx::PNGCodec::EncodeBGRASkBitmap(*image.ToSkBitmap(), false,
                                             &image_data)) {
        NOTREACHED() << "Could not encode extension favicon";
      }
      ForwardResult(base::RefCountedBytes::TakeVector(&image_data));
    } else {
      ForwardResult(NULL);
    }
  }

 private:
  ~ExtensionWebUIImageLoadingTracker() {}

  // Forwards the result on the request. If no favicon was available then
  // |icon_data| may be backed by NULL. Once the result has been forwarded the
  // instance is deleted.
  void ForwardResult(scoped_refptr<base::RefCountedMemory> icon_data) {
    history::FaviconData favicon;
    favicon.known_icon = icon_data.get() != NULL && icon_data->size() > 0;
    favicon.image_data = icon_data;
    favicon.icon_type = history::FAVICON;
    request_->ForwardResultAsync(request_->handle(), favicon);
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

ExtensionWebUI::ExtensionWebUI(content::WebUI* web_ui, const GURL& url)
    : WebUIController(web_ui),
      url_(url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  ExtensionService* service = profile->GetExtensionService();
  const Extension* extension =
      service->extensions()->GetExtensionOrAppByURL(ExtensionURLInfo(url));
  DCHECK(extension);
  // Only hide the url for internal pages (e.g. chrome-extension or packaged
  // component apps like bookmark manager.
  bool should_hide_url = !extension->is_hosted_app();

  // The base class defaults to enabling WebUI bindings, but we don't need
  // those (this is also reflected in ChromeWebUIControllerFactory::
  // UseWebUIBindingsForURL).
  int bindings = 0;

  // Bind externalHost to Extension WebUI loaded in Chrome Frame.
  const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
  if (browser_command_line.HasSwitch(switches::kChromeFrame))
    bindings |= content::BINDINGS_POLICY_EXTERNAL_HOST;
  // For chrome:// overrides, some of the defaults are a little different.
  GURL effective_url = web_ui->GetWebContents()->GetURL();
  if (effective_url.SchemeIs(chrome::kChromeUIScheme)) {
    if (effective_url.host() == chrome::kChromeUINewTabHost) {
      web_ui->FocusLocationBarByDefault();
    } else {
      // Current behavior of other chrome:// pages is to display the URL.
      should_hide_url = false;
    }
  }

  if (should_hide_url)
    web_ui->HideURL();

  web_ui->SetBindings(bindings);

  // Hack: A few things we specialize just for the bookmark manager.
  if (extension->id() == extension_misc::kBookmarkManagerId) {
    TabContents* tab = TabContents::FromWebContents(web_ui->GetWebContents());
    DCHECK(tab);
    bookmark_manager_extension_event_router_.reset(
        new BookmarkManagerExtensionEventRouter(profile, tab));

    web_ui->SetLinkTransitionType(content::PAGE_TRANSITION_AUTO_BOOKMARK);
  }
}

ExtensionWebUI::~ExtensionWebUI() {}

BookmarkManagerExtensionEventRouter*
ExtensionWebUI::bookmark_manager_extension_event_router() {
  return bookmark_manager_extension_event_router_.get();
}

////////////////////////////////////////////////////////////////////////////////
// chrome:// URL overrides

// static
void ExtensionWebUI::RegisterUserPrefs(PrefService* prefs) {
  prefs->RegisterDictionaryPref(kExtensionURLOverrides,
                                PrefService::UNSYNCABLE_PREF);
}

// static
bool ExtensionWebUI::HandleChromeURLOverride(
    GURL* url, content::BrowserContext* browser_context) {
  if (!url->SchemeIs(chrome::kChromeUIScheme))
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
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

    if (!url->query().empty())
      override += "?" + url->query();
    if (!url->ref().empty())
      override += "#" + url->ref();
    GURL extension_url(override);
    if (!extension_url.is_valid()) {
      NOTREACHED();
      UnregisterChromeURLOverride(page, profile, val);
      continue;
    }

    // Verify that the extension that's being referred to actually exists.
    const Extension* extension =
        service->extensions()->GetByID(extension_url.host());
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
        service->IsIncognitoEnabled(extension->id());
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
bool ExtensionWebUI::HandleChromeURLOverrideReverse(
    GURL* url, content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const DictionaryValue* overrides =
      profile->GetPrefs()->GetDictionary(kExtensionURLOverrides);
  if (!overrides)
    return false;

  // Find the reverse mapping based on the given URL. For example this maps the
  // internal URL chrome-extension://eemcgdkndhakfknomggombfjeno/main.html#1 to
  // chrome://bookmarks/#1 for display in the omnibox.
  for (DictionaryValue::key_iterator it = overrides->begin_keys(),
       end = overrides->end_keys(); it != end; ++it) {
    ListValue* url_list;
    if (!overrides->GetList(*it, &url_list))
      continue;

    for (ListValue::const_iterator it2 = url_list->begin(),
         end2 = url_list->end(); it2 != end2; ++it2) {
      std::string override;
      if (!(*it2)->GetAsString(&override))
        continue;
      if (StartsWithASCII(url->spec(), override, true)) {
        GURL original_url(chrome::kChromeUIScheme + std::string("://") + *it +
                          url->spec().substr(override.length()));
        *url = original_url;
        return true;
      }
    }
  }

  return false;
}

// static
void ExtensionWebUI::RegisterChromeURLOverrides(
    Profile* profile, const Extension::URLOverrideMap& overrides) {
  if (overrides.empty())
    return;

  PrefService* prefs = profile->GetPrefs();
  DictionaryPrefUpdate update(prefs, kExtensionURLOverrides);
  DictionaryValue* all_overrides = update.Get();

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
                                                  Profile* profile,
                                                  ListValue* list,
                                                  Value* override) {
  size_t index = 0;
  bool found = list->Remove(*override, &index);
  if (found && index == 0) {
    // This is the active override, so we need to find all existing
    // tabs for this override and get them to reload the original URL.
    for (TabContentsIterator iterator; !iterator.done(); ++iterator) {
      WebContents* tab = (*iterator)->web_contents();
      Profile* tab_profile =
          Profile::FromBrowserContext(tab->GetBrowserContext());
      if (tab_profile != profile)
        continue;

      GURL url = tab->GetURL();
      if (!url.SchemeIs(chrome::kChromeUIScheme) || url.host() != page)
        continue;

      // Don't use Reload() since |url| isn't the same as the internal URL
      // that NavigationController has.
      tab->GetController().LoadURL(
          url, content::Referrer(url, WebKit::WebReferrerPolicyDefault),
          content::PAGE_TRANSITION_RELOAD, std::string());
    }
  }
}

// static
void ExtensionWebUI::UnregisterChromeURLOverride(const std::string& page,
    Profile* profile, Value* override) {
  if (!override)
    return;
  PrefService* prefs = profile->GetPrefs();
  DictionaryPrefUpdate update(prefs, kExtensionURLOverrides);
  DictionaryValue* all_overrides = update.Get();
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
  DictionaryPrefUpdate update(prefs, kExtensionURLOverrides);
  DictionaryValue* all_overrides = update.Get();
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
