// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_web_ui.h"

#include <set>
#include <vector>

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_util.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/url_constants.h"
#include "components/favicon_base/favicon_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "content/public/common/page_transition_types.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/image_loader.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_icon_set.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handlers/icons_handler.h"
#include "extensions/common/manifest_handlers/incognito_info.h"
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
void CleanUpDuplicates(base::ListValue* list) {
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
void UnregisterAndReplaceOverrideForWebContents(const std::string& page,
                                                Profile* profile,
                                                WebContents* web_contents) {
  if (Profile::FromBrowserContext(web_contents->GetBrowserContext()) != profile)
    return;

  GURL url = web_contents->GetURL();
  if (!url.SchemeIs(content::kChromeUIScheme) || url.host() != page)
    return;

  // Don't use Reload() since |url| isn't the same as the internal URL that
  // NavigationController has.
  web_contents->GetController().LoadURL(
      url, content::Referrer(url, blink::WebReferrerPolicyDefault),
      content::PAGE_TRANSITION_RELOAD, std::string());
}

// Run favicon callbck with image result. If no favicon was available then
// |image| will be empty.
void RunFaviconCallbackAsync(
    const favicon_base::FaviconResultsCallback& callback,
    const gfx::Image& image) {
  std::vector<favicon_base::FaviconRawBitmapResult>* favicon_bitmap_results =
      new std::vector<favicon_base::FaviconRawBitmapResult>();

  const std::vector<gfx::ImageSkiaRep>& image_reps =
      image.AsImageSkia().image_reps();
  for (size_t i = 0; i < image_reps.size(); ++i) {
    const gfx::ImageSkiaRep& image_rep = image_reps[i];
    scoped_refptr<base::RefCountedBytes> bitmap_data(
        new base::RefCountedBytes());
    if (gfx::PNGCodec::EncodeBGRASkBitmap(image_rep.sk_bitmap(),
                                          false,
                                          &bitmap_data->data())) {
      favicon_base::FaviconRawBitmapResult bitmap_result;
      bitmap_result.bitmap_data = bitmap_data;
      bitmap_result.pixel_size = gfx::Size(image_rep.pixel_width(),
                                            image_rep.pixel_height());
      // Leave |bitmap_result|'s icon URL as the default of GURL().
      bitmap_result.icon_type = favicon_base::FAVICON;

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

bool ValidateOverrideURL(const base::Value* override_url_value,
                         const GURL& source_url,
                         const extensions::ExtensionSet& extensions,
                         GURL* override_url,
                         const Extension** extension) {
  std::string override;
  if (!override_url_value || !override_url_value->GetAsString(&override)) {
    return false;
  }
  if (!source_url.query().empty())
    override += "?" + source_url.query();
  if (!source_url.ref().empty())
    override += "#" + source_url.ref();
  *override_url = GURL(override);
  if (!override_url->is_valid()) {
    return false;
  }
  *extension = extensions.GetByID(override_url->host());
  if (!*extension) {
    return false;
  }
  return true;
}

}  // namespace

const char ExtensionWebUI::kExtensionURLOverrides[] =
    "extensions.chrome_url_overrides";

ExtensionWebUI::ExtensionWebUI(content::WebUI* web_ui, const GURL& url)
    : WebUIController(web_ui),
      url_(url) {
  Profile* profile = Profile::FromWebUI(web_ui);
  const Extension* extension = extensions::ExtensionRegistry::Get(
      profile)->enabled_extensions().GetExtensionOrAppByURL(url);
  DCHECK(extension);

  // The base class defaults to enabling WebUI bindings, but we don't need
  // those (this is also reflected in ChromeWebUIControllerFactory::
  // UseWebUIBindingsForURL).
  int bindings = 0;
  web_ui->SetBindings(bindings);

  // Hack: A few things we specialize just for the bookmark manager.
  if (extension->id() == extension_misc::kBookmarkManagerId) {
    bookmark_manager_private_drag_event_router_.reset(
        new extensions::BookmarkManagerPrivateDragEventRouter(
            profile, web_ui->GetWebContents()));

    web_ui->SetLinkTransitionType(content::PAGE_TRANSITION_AUTO_BOOKMARK);
  }
}

ExtensionWebUI::~ExtensionWebUI() {}

extensions::BookmarkManagerPrivateDragEventRouter*
ExtensionWebUI::bookmark_manager_private_drag_event_router() {
  return bookmark_manager_private_drag_event_router_.get();
}

////////////////////////////////////////////////////////////////////////////////
// chrome:// URL overrides

// static
void ExtensionWebUI::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(
      kExtensionURLOverrides,
      user_prefs::PrefRegistrySyncable::UNSYNCABLE_PREF);
}

// static
bool ExtensionWebUI::HandleChromeURLOverride(
    GURL* url,
    content::BrowserContext* browser_context) {
  if (!url->SchemeIs(content::kChromeUIScheme))
    return false;

  Profile* profile = Profile::FromBrowserContext(browser_context);
  const base::DictionaryValue* overrides =
      profile->GetPrefs()->GetDictionary(kExtensionURLOverrides);

  std::string url_host = url->host();
  const base::ListValue* url_list = NULL;
  if (!overrides || !overrides->GetList(url_host, &url_list))
    return false;

  extensions::ExtensionRegistry* registry =
      extensions::ExtensionRegistry::Get(browser_context);
  const extensions::ExtensionSet& extensions = registry->enabled_extensions();

  GURL component_url;
  bool found_component_override = false;

  // Iterate over the URL list looking for a suitable override. If a
  // valid non-component override is encountered it is chosen immediately.
  for (size_t i = 0; i < url_list->GetSize(); ++i) {
    const base::Value* val = NULL;
    url_list->Get(i, &val);

    GURL override_url;
    const Extension* extension;
    if (!ValidateOverrideURL(
            val, *url, extensions, &override_url, &extension)) {
      LOG(WARNING) << "Invalid chrome URL override";
      UnregisterChromeURLOverride(url_host, profile, val);
      // The above Unregister call will remove this item from url_list.
      --i;
      continue;
    }

    // We can't handle chrome-extension URLs in incognito mode unless the
    // extension uses split mode.
    bool incognito_override_allowed =
        extensions::IncognitoInfo::IsSplitMode(extension) &&
        extensions::util::IsIncognitoEnabled(extension->id(), profile);
    if (profile->IsOffTheRecord() && !incognito_override_allowed) {
      continue;
    }

    if (!extensions::Manifest::IsComponentLocation(extension->location())) {
      *url = override_url;
      return true;
    }

    if (!found_component_override) {
      found_component_override = true;
      component_url = override_url;
    }
  }

  // If no other non-component overrides were found, use the first known
  // component override, if any.
  if (found_component_override) {
    *url = component_url;
    return true;
  }

  return false;
}

// static
bool ExtensionWebUI::HandleChromeURLOverrideReverse(
    GURL* url, content::BrowserContext* browser_context) {
  Profile* profile = Profile::FromBrowserContext(browser_context);
  const base::DictionaryValue* overrides =
      profile->GetPrefs()->GetDictionary(kExtensionURLOverrides);
  if (!overrides)
    return false;

  // Find the reverse mapping based on the given URL. For example this maps the
  // internal URL
  // chrome-extension://eemcgdkfndhakfknompkggombfjjjeno/main.html#1 to
  // chrome://bookmarks/#1 for display in the omnibox.
  for (base::DictionaryValue::Iterator it(*overrides); !it.IsAtEnd();
       it.Advance()) {
    const base::ListValue* url_list = NULL;
    if (!it.value().GetAsList(&url_list))
      continue;

    for (base::ListValue::const_iterator it2 = url_list->begin();
         it2 != url_list->end(); ++it2) {
      std::string override;
      if (!(*it2)->GetAsString(&override))
        continue;
      if (StartsWithASCII(url->spec(), override, true)) {
        GURL original_url(content::kChromeUIScheme + std::string("://") +
                          it.key() + url->spec().substr(override.length()));
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
  base::DictionaryValue* all_overrides = update.Get();

  // For each override provided by the extension, add it to the front of
  // the override list if it's not already in the list.
  URLOverrides::URLOverrideMap::const_iterator iter = overrides.begin();
  for (; iter != overrides.end(); ++iter) {
    const std::string& key = iter->first;
    base::ListValue* page_overrides = NULL;
    if (!all_overrides->GetList(key, &page_overrides)) {
      page_overrides = new base::ListValue();
      all_overrides->Set(key, page_overrides);
    } else {
      CleanUpDuplicates(page_overrides);

      // Verify that the override isn't already in the list.
      base::ListValue::iterator i = page_overrides->begin();
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
    page_overrides->Insert(0, new base::StringValue(iter->second.spec()));
  }
}

// static
void ExtensionWebUI::UnregisterAndReplaceOverride(const std::string& page,
                                                  Profile* profile,
                                                  base::ListValue* list,
                                                  const base::Value* override) {
  size_t index = 0;
  bool found = list->Remove(*override, &index);
  if (found && index == 0) {
    // This is the active override, so we need to find all existing
    // tabs for this override and get them to reload the original URL.
    base::Callback<void(WebContents*)> callback =
        base::Bind(&UnregisterAndReplaceOverrideForWebContents, page, profile);
    extensions::ExtensionTabUtil::ForEachTab(callback);
  }
}

// static
void ExtensionWebUI::UnregisterChromeURLOverride(const std::string& page,
                                                 Profile* profile,
                                                 const base::Value* override) {
  if (!override)
    return;
  PrefService* prefs = profile->GetPrefs();
  DictionaryPrefUpdate update(prefs, kExtensionURLOverrides);
  base::DictionaryValue* all_overrides = update.Get();
  base::ListValue* page_overrides = NULL;
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
  base::DictionaryValue* all_overrides = update.Get();
  URLOverrides::URLOverrideMap::const_iterator iter = overrides.begin();
  for (; iter != overrides.end(); ++iter) {
    const std::string& page = iter->first;
    base::ListValue* page_overrides = NULL;
    if (!all_overrides->GetList(page, &page_overrides)) {
      // If it's being unregistered, it should already be in the list.
      NOTREACHED();
      continue;
    } else {
      base::StringValue override(iter->second.spec());
      UnregisterAndReplaceOverride(iter->first, profile,
                                   page_overrides, &override);
    }
  }
}

// static
void ExtensionWebUI::GetFaviconForURL(
    Profile* profile,
    const GURL& page_url,
    const favicon_base::FaviconResultsCallback& callback) {
  const Extension* extension = extensions::ExtensionRegistry::Get(
      profile)->enabled_extensions().GetByID(page_url.host());
  if (!extension) {
    RunFaviconCallbackAsync(callback, gfx::Image());
    return;
  }

  // Fetch resources for all supported scale factors for which there are
  // resources. Load image reps for all supported scale factors (in addition to
  // 1x) immediately instead of in an as needed fashion to be consistent with
  // how favicons are requested for chrome:// and page URLs.
  const std::vector<float>& favicon_scales = favicon_base::GetFaviconScales();
  std::vector<extensions::ImageLoader::ImageRepresentation> info_list;
  for (size_t i = 0; i < favicon_scales.size(); ++i) {
    float scale = favicon_scales[i];
    int pixel_size = static_cast<int>(gfx::kFaviconSize * scale);
    extensions::ExtensionResource icon_resource =
        extensions::IconsInfo::GetIconResource(extension,
                                               pixel_size,
                                               ExtensionIconSet::MATCH_BIGGER);

    ui::ScaleFactor resource_scale_factor = ui::GetSupportedScaleFactor(scale);
    info_list.push_back(extensions::ImageLoader::ImageRepresentation(
        icon_resource,
        extensions::ImageLoader::ImageRepresentation::ALWAYS_RESIZE,
        gfx::Size(pixel_size, pixel_size),
        resource_scale_factor));
  }

  // LoadImagesAsync actually can run callback synchronously. We want to force
  // async.
  extensions::ImageLoader::Get(profile)->LoadImagesAsync(
      extension, info_list, base::Bind(&RunFaviconCallbackAsync, callback));
}
