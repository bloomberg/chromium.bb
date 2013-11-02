// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/install_module_verifier_win.h"

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/md5.h"
#include "base/memory/scoped_ptr.h"
#include "base/metrics/sparse_histogram.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_tokenizer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/enumerate_modules_model_win.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "grit/browser_resources.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

struct { size_t id; const char* module_name_digest; }
  kExpectedInstallModules[] = {
    {1u, "c8cc47613e155f2129f480c6ced84549"},  // chrome.dll
    {2u, "49b78a23b0d8d5d8fb60d4e472b22764"},  // chrome_child.dll
  };

// Helper to extract canonical loaded module names from the EnumerateModulesWin
// output and then verify the results.
void VerifyEnumeratedModules(const base::ListValue& module_list) {
  std::set<std::string> module_name_digests;
  ExtractLoadedModuleNameDigests(module_list, &module_name_digests);

  AdditionalModules additional_modules;
  ParseAdditionalModules(
      ResourceBundle::GetSharedInstance().GetRawDataResource(
          IDR_ADDITIONAL_MODULES_LIST),
      &additional_modules);

  std::set<size_t> installed_module_ids;
  VerifyModules(module_name_digests, additional_modules, &installed_module_ids);
  for (std::set<size_t>::iterator it = installed_module_ids.begin();
       it != installed_module_ids.end();
       ++it) {
    UMA_HISTOGRAM_SPARSE_SLOWLY("InstallVerifier.ModuleMatch", *it);
  }
}

// Waits for NOTIFICATION_MODULE_LIST_ENUMERATED, which indicates that
// EnumerateModulesWin has completed its work. Retrieves the enumerated module
// list and processes it.
class InstallModuleVerifier : public content::NotificationObserver {
 public:
  // Creates an instance that will wait for module enumeration to complete,
  // process the results, and then delete itself.
  static void WaitForModuleList() {
    // Will delete itself when scan results are available.
    new InstallModuleVerifier();
  }

 private:
  InstallModuleVerifier() {
    notification_registrar_.Add(this,
                                chrome::NOTIFICATION_MODULE_LIST_ENUMERATED,
                                content::NotificationService::AllSources());
  }

  ~InstallModuleVerifier() {}

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(type, chrome::NOTIFICATION_MODULE_LIST_ENUMERATED);
    EnumerateModulesModel* model =
        content::Source<EnumerateModulesModel>(source).ptr();
    scoped_ptr<base::ListValue> module_list(model->GetModuleList());

    if (module_list.get())
      VerifyEnumeratedModules(*module_list);

    delete this;
  }

  content::NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(InstallModuleVerifier);
};

}  // namespace

void BeginModuleVerification() {
  scoped_ptr<base::ListValue> module_list(
      EnumerateModulesModel::GetInstance()->GetModuleList());
  if (module_list.get()) {
    VerifyEnumeratedModules(*module_list);
  } else {
    InstallModuleVerifier::WaitForModuleList();
    EnumerateModulesModel::GetInstance()->ScanNow();
  }
}

void ExtractLoadedModuleNameDigests(
    const base::ListValue& module_list,
    std::set<std::string>* module_name_digests) {
  DCHECK(module_name_digests);

  // EnumerateModulesModel produces a list of dictionaries.
  // Each dictionary corresponds to a module and exposes a number of properties.
  // We care only about 'type' and 'name'.
  for (size_t i = 0; i < module_list.GetSize(); ++i) {
    const base::DictionaryValue* module_dictionary = NULL;
    if (!module_list.GetDictionary(i, &module_dictionary))
      continue;
    ModuleEnumerator::ModuleType module_type =
        ModuleEnumerator::LOADED_MODULE;
    if (!module_dictionary->GetInteger(
            "type", reinterpret_cast<int*>(&module_type)) ||
        module_type != ModuleEnumerator::LOADED_MODULE) {
      continue;
    }
    std::string module_name;
    if (!module_dictionary->GetString("name", &module_name))
      continue;
    StringToLowerASCII(&module_name);
    module_name_digests->insert(base::MD5String(module_name));
  }
}

void VerifyModules(
    const std::set<std::string>& module_name_digests,
    const AdditionalModules& additional_modules,
    std::set<size_t>* installed_module_ids) {
  DCHECK(installed_module_ids);

  for (size_t i = 0; i < arraysize(kExpectedInstallModules); ++i) {
    if (module_name_digests.find(
            kExpectedInstallModules[i].module_name_digest) !=
        module_name_digests.end()) {
      installed_module_ids->insert(kExpectedInstallModules[i].id);
    }
  }

  for (AdditionalModules::const_iterator it = additional_modules.begin();
       it != additional_modules.end(); ++it) {
    std::string additional_module = it->second.as_string();
    StringToLowerASCII(&additional_module);

    if (module_name_digests.find(additional_module)
        != module_name_digests.end()) {
      installed_module_ids->insert(it->first);
    }
  }
}

namespace {

// Parses a line consisting of a positive decimal number and a 32-digit
// hexadecimal number, separated by a space. Inserts the output, if valid, into
// |additional_modules|. Unexpected leading or trailing characters will cause
// the line to be ignored, as will invalid decimal/hexadecimal characters.
void ParseAdditionalModuleLine(
    const base::StringPiece& line,
    AdditionalModules* additional_modules) {
  DCHECK(additional_modules);

  base::CStringTokenizer line_tokenizer(line.begin(), line.end(), " ");

  if (!line_tokenizer.GetNext())
    return;  // Empty string.
  base::StringPiece id_piece(line_tokenizer.token_piece());

  if (!line_tokenizer.GetNext())
    return;  // No delimiter (' ').
  base::StringPiece digest_piece(line_tokenizer.token_piece());

  if (line_tokenizer.GetNext())
    return;  // Unexpected trailing characters.

  unsigned id = 0;
  if (!StringToUint(id_piece, &id))
    return;  // First token was not decimal.

  if (digest_piece.length() != 32)
    return;  // Second token is not the right length.

  for (base::StringPiece::const_iterator it = digest_piece.begin();
       it != digest_piece.end(); ++it) {
    if (!IsHexDigit(*it))
      return;  // Second token has invalid characters.
  }

  // This is a valid line.
  additional_modules->push_back(std::make_pair(id, digest_piece));
}

}  // namespace

void ParseAdditionalModules(
    const base::StringPiece& raw_data,
    AdditionalModules* additional_modules) {
  DCHECK(additional_modules);

  base::CStringTokenizer file_tokenizer(raw_data.begin(),
                                        raw_data.end(),
                                        "\r\n");
  while (file_tokenizer.GetNext()) {
    ParseAdditionalModuleLine(base::StringPiece(file_tokenizer.token_piece()),
                              additional_modules);
  }
}
