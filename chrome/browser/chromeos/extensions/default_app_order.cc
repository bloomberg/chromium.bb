// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/default_app_order.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/chromeos_paths.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/common/constants.h"

namespace chromeos {
namespace default_app_order {
namespace {

// The single ExternalLoader instance.
ExternalLoader* loader_instance = NULL;

// Names used in JSON file.
const char kOemAppsFolderAttr[] = "oem_apps_folder";
const char kLocalizedContentAttr[] = "localized_content";
const char kDefaultAttr[] = "default";
const char kNameAttr[] = "name";
const char kImportDefaultOrderAttr[] = "import_default_order";

const char* kDefaultAppOrder[] = {
    extension_misc::kChromeAppId,
    extensions::kWebStoreAppId,
    extension_misc::kGoogleSearchAppId,
    extension_misc::kYoutubeAppId,
    extension_misc::kGmailAppId,
    "ejjicmeblgpmajnghnpcppodonldlgfn",  // Calendar
    "kjebfhglflhjjjiceimfkgicifkhjlnm",  // Scratchpad
    "lneaknkopdijkpnocmklfnjbeapigfbh",  // Google Maps
    "apdfllckaahabafndbhieahigkjlhalf",  // Drive
    extension_misc::kGoogleDocAppId,
    extension_misc::kGoogleSheetsAppId,
    extension_misc::kGoogleSlidesAppId,
    "dlppkpafhbajpcmmoheippocdidnckmm",  // Google+
    "kbpgddbgniojgndnhlkjbkpknjhppkbk",  // Google+ Hangouts
    "hhaomjibdihmijegdhdafkllkbggdgoj",  // Files
    extension_misc::kGooglePlayMusicAppId,
    "mmimngoggfoobjdlefbcabngfnmieonb",  // Play Books
    "gdijeikdkaembjbdobgfkoidjkpbmlkd",  // Play Movies & TV
    "fobcpibfeplaikcclojfdhfdmbbeofai",  // Games
    "joodangkbfjnajiiifokapkpmhfnpleo",  // Calculator
    "hfhhnacclhffhdffklopdkcgdhifgngh",  // Camera
    "gbchcmhmhahfdphkhkmpfmihenigjmpp",  // Chrome Remote Desktop
};

// Reads external ordinal json file and returned the parsed value. Returns NULL
// if the file does not exist or could not be parsed properly. Caller takes
// ownership of the returned value.
base::ListValue* ReadExternalOrdinalFile(const base::FilePath& path) {
  if (!base::PathExists(path))
    return NULL;

  JSONFileValueSerializer serializer(path);
  std::string error_msg;
  base::Value* value = serializer.Deserialize(NULL, &error_msg);
  if (!value) {
    LOG(WARNING) << "Unable to deserialize default app ordinals json data:"
        << error_msg << ", file=" << path.value();
    return NULL;
  }

  base::ListValue* ordinal_list_value = NULL;
  if (value->GetAsList(&ordinal_list_value))
    return ordinal_list_value;

  LOG(WARNING) << "Expect a JSON list in file " << path.value();
  return NULL;
}

std::string GetLocaleSpecificStringImpl(
    const base::DictionaryValue* root,
    const std::string& locale,
    const std::string& dictionary_name,
    const std::string& entry_name) {
  const base::DictionaryValue* dictionary_content = NULL;
  if (!root || !root->GetDictionary(dictionary_name, &dictionary_content))
    return std::string();

  const base::DictionaryValue* locale_dictionary = NULL;
  if (dictionary_content->GetDictionary(locale, &locale_dictionary)) {
    std::string result;
    if (locale_dictionary->GetString(entry_name, &result))
      return result;
  }

  const base::DictionaryValue* default_dictionary = NULL;
  if (dictionary_content->GetDictionary(kDefaultAttr, &default_dictionary)) {
    std::string result;
    if (default_dictionary->GetString(entry_name, &result))
      return result;
  }

  return std::string();
}

// Gets built-in default app order.
void GetDefault(std::vector<std::string>* app_ids) {
  for (size_t i = 0; i < arraysize(kDefaultAppOrder); ++i)
    app_ids->push_back(std::string(kDefaultAppOrder[i]));
}

}  // namespace

const size_t kDefaultAppOrderCount = arraysize(kDefaultAppOrder);

ExternalLoader::ExternalLoader(bool async)
    : loaded_(true /* manual_rest */, false /* initially_signaled */) {
  DCHECK(!loader_instance);
  loader_instance = this;

  if (async) {
    content::BrowserThread::PostBlockingPoolTask(FROM_HERE,
        base::Bind(&ExternalLoader::Load, base::Unretained(this)));
  } else {
    Load();
  }
}

ExternalLoader::~ExternalLoader() {
  DCHECK(loaded_.IsSignaled());
  DCHECK_EQ(loader_instance, this);
  loader_instance = NULL;
}

const std::vector<std::string>& ExternalLoader::GetAppIds() {
  if (!loaded_.IsSignaled())
    LOG(ERROR) << "GetAppIds() called before loaded.";
  return app_ids_;
}

const std::string& ExternalLoader::GetOemAppsFolderName() {
  if (!loaded_.IsSignaled())
    LOG(ERROR) << "GetOemAppsFolderName() called before loaded.";
  return oem_apps_folder_name_;
}

void ExternalLoader::Load() {
  base::FilePath ordinals_file;
  CHECK(PathService::Get(chromeos::FILE_DEFAULT_APP_ORDER, &ordinals_file));

  scoped_ptr<base::ListValue> ordinals_value(
      ReadExternalOrdinalFile(ordinals_file));
  if (ordinals_value) {
    std::string locale = g_browser_process->GetApplicationLocale();
    for (size_t i = 0; i < ordinals_value->GetSize(); ++i) {
      std::string app_id;
      base::DictionaryValue* dict = NULL;
      if (ordinals_value->GetString(i, &app_id)) {
        app_ids_.push_back(app_id);
      } else if (ordinals_value->GetDictionary(i, &dict)) {
        bool flag = false;
        if (dict->GetBoolean(kOemAppsFolderAttr, &flag) && flag) {
          oem_apps_folder_name_ = GetLocaleSpecificStringImpl(
              dict, locale, kLocalizedContentAttr, kNameAttr);
        } else if (dict->GetBoolean(kImportDefaultOrderAttr, &flag) && flag) {
          GetDefault(&app_ids_);
        } else {
          LOG(ERROR) << "Invalid syntax in default_app_order.json";
        }
      } else {
        LOG(ERROR) << "Invalid entry in default_app_order.json";
      }
    }
  } else {
    GetDefault(&app_ids_);
  }

  loaded_.Signal();
}

void Get(std::vector<std::string>* app_ids) {
  // |loader_instance| could be NULL for test.
  if (!loader_instance) {
    GetDefault(app_ids);
    return;
  }

  *app_ids = loader_instance->GetAppIds();
}

std::string GetOemAppsFolderName() {
  // |loader_instance| could be NULL for test.
  if (!loader_instance)
    return std::string();
  else
    return loader_instance->GetOemAppsFolderName();
}

}  // namespace default_app_order
}  // namespace chromeos
