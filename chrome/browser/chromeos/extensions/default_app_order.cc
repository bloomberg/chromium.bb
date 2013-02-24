// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/default_app_order.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/time.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_thread.h"

namespace chromeos {
namespace default_app_order {

namespace {

// The single ExternalLoader instance.
ExternalLoader* loader_instance = NULL;

// Reads external ordinal json file and returned the parsed value. Returns NULL
// if the file does not exist or could not be parsed properly. Caller takes
// ownership of the returned value.
base::ListValue* ReadExternalOrdinalFile(const base::FilePath& path) {
  if (!file_util::PathExists(path))
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

// Gets built-in default app order.
void GetDefault(std::vector<std::string>* app_ids) {
  DCHECK(app_ids && app_ids->empty());

  const char* kDefaultAppOrder[] = {
    extension_misc::kChromeAppId,
    extension_misc::kWebStoreAppId,
    "coobgpohoikkiipiblmjeljniedjpjpf",  // Search
    "blpcfgokakmgnkcojhhkbfbldkacnbeo",  // Youtube
    "pjkljhegncpnkpknbcohdijeoejaedia",  // Gmail
    "ejjicmeblgpmajnghnpcppodonldlgfn",  // Calendar
    "kjebfhglflhjjjiceimfkgicifkhjlnm",  // Scratchpad
    "lneaknkopdijkpnocmklfnjbeapigfbh",  // Google Maps
    "apdfllckaahabafndbhieahigkjlhalf",  // Drive
    "aohghmighlieiainnegkcijnfilokake",  // Docs
    "felcaaldnbdncclmgdcncolpebgiejap",  // Sheets
    "aapocclcgogkmnckokdopfmhonfmgoek",  // Slides
    "dlppkpafhbajpcmmoheippocdidnckmm",  // Google+
    "kbpgddbgniojgndnhlkjbkpknjhppkbk",  // Google+ Hangouts
    "hhaomjibdihmijegdhdafkllkbggdgoj",  // Files
    "hkhhlkdconhgemhegnplaldnmnmkaemd",  // Tips & Tricks
    "icppfcnhkcmnfdhfhphakoifcfokfdhg",  // Play Music
    "mmimngoggfoobjdlefbcabngfnmieonb",  // Play Books
    "fppdphmgcddhjeddoeghpjefkdlccljb",  // Play Movies
    "fobcpibfeplaikcclojfdhfdmbbeofai",  // Games
    "joodangkbfjnajiiifokapkpmhfnpleo",  // Calculator
    "hfhhnacclhffhdffklopdkcgdhifgngh",  // Camera
    "gbchcmhmhahfdphkhkmpfmihenigjmpp",  // Chrome Remote Desktop
  };

  for (size_t i = 0; i < arraysize(kDefaultAppOrder); ++i)
    app_ids->push_back(std::string(kDefaultAppOrder[i]));
}

}  // namespace

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
  CHECK(loaded_.IsSignaled());
  return app_ids_;
}

void ExternalLoader::Load() {
  base::FilePath ordinals_file;
  CHECK(PathService::Get(chrome::FILE_DEFAULT_APP_ORDER, &ordinals_file));

  scoped_ptr<base::ListValue> ordinals_value(
      ReadExternalOrdinalFile(ordinals_file));
  if (ordinals_value) {
    for (size_t i = 0; i < ordinals_value->GetSize(); ++i) {
      std::string app_id;
      CHECK(ordinals_value->GetString(i, &app_id));
      app_ids_.push_back(app_id);
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

}  // namespace default_app_order
}  // namespace chromeos
