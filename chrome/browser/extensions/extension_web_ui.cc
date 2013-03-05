// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui.h"

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/image_loader.h"
#include "chrome/browser/favicon/favicon_util.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/icons/icons_handler.h"
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
#include "ui/gfx/image/image_skia.h"

using content::WebContents;
using extensions::Extension;
using extensions::URLOverrides;

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

// Reloads the page in |web_contents| if it uses the same profile as |profile|
// and if the current URL is a chrome URL.
void UnregisterAndReplaceOverrideForWebContents(
    const std::string& page, Profile* profile, WebContents* web_contents) {
  if (Profile::FromBrowserContext(web_contents->GetBrowserContext()) != profile)
    return;

  GURL url = web_contents->GetURL();
  if (!url.SchemeIs(chrome::kChromeUIScheme) || url.host() != page)
    return;

  // Don't use Reload() since |url| isn't the same as the internal URL that
  // NavigationController has.
  web_contents->GetController().LoadURL(
      url, content::Referrer(url, WebKit::WebReferrerPolicyDefault),
      content::PAGE_TRANSITION_RELOAD, std::string());
}

// Run favicon callbck with image result. If no favicon was available then
// |image| will be empty.
void RunFaviconCallbackAsync(
    const FaviconService::FaviconResultsCallback& callback,
    const gfx::Image& image) {
  std::vector<history::FaviconBitmapResult>* favicon_bitmap_results =
      new std::vector<history::FaviconBitmapResult>();

  const std::vector<gfx::ImageSkiaRep>& image_reps =
      image.AsImageSkia().image_reps();
  for (size_t i = 0; i < image_reps.size(); ++i) {
    const gfx::ImageSkiaRep& image_rep = image_reps[i];
    scoped_refptr<base::RefCountedBytes> bitmap_data(
        new base::RefCountedBytes());
    if (gfx::PNGCodec::EncodeBGRASkBitmap(image_rep.sk_bitmap(),
                                          false,
                                          &bitmap_data->data())) {
      history::FaviconBitmapResult bitmap_result;
      bitmap_result.bitmap_data = bitmap_data;
      bitmap_result.pixel_size = gfx::Size(image_rep.pixel_width(),
                                            image_rep.pixel_height());
      // Leave |bitmap_result|'s icon URL as the default of GURL().
      bitmap_result.icon_type = history::FAVICON;

      favicon_bitmap_results->push_back(bitmap_result);
    } else {
      NOTREACHED() << "Could not encode extension favicon";
    }
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(&FaviconService::FaviconResultsCallbackRunner,
                 callback,
                 base::Owned(favicon_bitmap_results)));
}

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
    bookmark_manager_private_event_router_.reset(
        new extensions::BookmarkManagerPrivateEventRouter(
            profile, web_ui->GetWebContents()));

    web_ui->SetLinkTransitionType(content::PAGE_TRANSITION_AUTO_BOOKMARK);
  }
}

ExtensionWebUI::~ExtensionWebUI() {}

extensions::BookmarkManagerPrivateEventRouter*
ExtensionWebUI::bookmark_manager_private_event_router() {
  return bookmark_manager_private_event_router_.get();
}

////////////////////////////////////////////////////////////////////////////////
// chrome:// URL overrides

// static
void ExtensionWebUI::RegisterUserPrefs(PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(kExtensionURLOverrides,
                                   PrefRegistrySyncable::UNSYNCABLE_PREF);
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
  const ListValue* url_list;
  if (!overrides || !overrides->GetList(page, &url_list))
    return false;

  ExtensionService* service = profile->GetExtensionService();

  size_t i = 0;
  while (i < url_list->GetSize()) {
    const Value* val = NULL;
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
  // internal URL
  // chrome-extension://eemcgdkfndhakfknompkggombfjjjeno/main.html#1 to
  // chrome://bookmarks/#1 for display in the omnibox.
  for (DictionaryValue::key_iterator it = overrides->begin_keys(),
       end = overrides->end_keys(); it != end; ++it) {
    const ListValue* url_list;
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
    Profile* profile, const URLOverrides::URLOverrideMap& overrides) {
  if (overrides.empty())
    return;

  PrefService* prefs = profile->GetPrefs();
  DictionaryPrefUpdate update(prefs, kExtensionURLOverrides);
  DictionaryValue* all_overrides = update.Get();

  // For each override provided by the extension, add it to the front of
  // the override list if it's not already in the list.
  URLOverrides::URLOverrideMap::const_iterator iter = overrides.begin();
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
                                                  const Value* override) {
  size_t index = 0;
  bool found = list->Remove(*override, &index);
  if (found && index == 0) {
    // This is the active override, so we need to find all existing
    // tabs for this override and get them to reload the original URL.
    base::Callback<void(WebContents*)> callback =
        base::Bind(&UnregisterAndReplaceOverrideForWebContents, page, profile);
    ExtensionTabUtil::ForEachTab(callback);
  }
}

// static
void ExtensionWebUI::UnregisterChromeURLOverride(const std::string& page,
                                                 Profile* profile,
                                                 const Value* override) {
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
    Profile* profile, const URLOverrides::URLOverrideMap& overrides) {
  if (overrides.empty())
    return;
  PrefService* prefs = profile->GetPrefs();
  DictionaryPrefUpdate update(prefs, kExtensionURLOverrides);
  DictionaryValue* all_overrides = update.Get();
  URLOverrides::URLOverrideMap::const_iterator iter = overrides.begin();
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
void ExtensionWebUI::GetFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    const FaviconService::FaviconResultsCallback& callback) {
  // Even when the extensions service is enabled by default, it's still
  // disabled in incognito mode.
  ExtensionService* service = profile->GetExtensionService();
  if (!service) {
    RunFaviconCallbackAsync(callback, gfx::Image());
    return;
  }
  const Extension* extension = service->extensions()->GetByID(page_url.host());
  if (!extension) {
    RunFaviconCallbackAsync(callback, gfx::Image());
    return;
  }

  // Fetch resources for all supported scale factors for which there are
  // resources. Load image reps for all supported scale factors (in addition to
  // 1x) immediately instead of in an as needed fashion to be consistent with
  // how favicons are requested for chrome:// and page URLs.
  const std::vector<ui::ScaleFactor>& scale_factors =
      FaviconUtil::GetFaviconScaleFactors();
  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;
  for (size_t i = 0; i < scale_factors.size(); ++i) {
    float scale = ui::GetScaleFactorScale(scale_factors[i]);
    int pixel_size = static_cast<int>(gfx::kFaviconSize * scale);
    ExtensionResource icon_resource =
        extensions::IconsInfo::GetIconResource(extension,
                                               pixel_size,
                                               ExtensionIconSet::MATCH_BIGGER);

    info_list.push_back(
        extensions::ImageLoader::ImageRepresentation(
            icon_resource,
            extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
            gfx::Size(pixel_size, pixel_size),
            scale_factors[i]));
  }

  // LoadImagesAsync actually can run callback synchronously. We want to force
  // async.
  extensions::ImageLoader::Get(profile)->LoadImagesAsync(
      extension, info_list, base::Bind(&RunFaviconCallbackAsync, callback));
}
