// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "input_method_event_router.h"

#include <algorithm>

#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/values.h"
#include "chrome/browser/chromeos/web_socket_proxy_controller.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile.h"

namespace {

// Prefix, which is used by XKB.
const char kXkbPrefix[] = "xkb:";

// Extension ID which is used in browser_tests.
const char kInputMethodTestExtensionID[] = "ilanclmaeigfpnmdlgelmhkpkegdioip";

class InputMethodPrivateExtensionsWhitelist {
 public:
  InputMethodPrivateExtensionsWhitelist() {
    chromeos::FillWithExtensionsIdsWithPrivateAccess(&ids_);
    ids_.push_back(kInputMethodTestExtensionID);
    std::sort(ids_.begin(), ids_.end());
  }

  bool HasId(const std::string& id) {
    return std::binary_search(ids_.begin(), ids_.end(), id);
  }

  const std::vector<std::string>& ids() { return ids_; }

 private:
  std::vector<std::string> ids_;
};

base::LazyInstance<InputMethodPrivateExtensionsWhitelist>
    g_input_method_private_extensions_whitelist(base::LINKER_INITIALIZED);

}  // namespace

namespace chromeos {

// static
ExtensionInputMethodEventRouter*
    ExtensionInputMethodEventRouter::GetInstance() {
  return Singleton<ExtensionInputMethodEventRouter>::get();
}

ExtensionInputMethodEventRouter::ExtensionInputMethodEventRouter() {
  input_method::InputMethodManager::GetInstance()->AddObserver(this);
}

ExtensionInputMethodEventRouter::~ExtensionInputMethodEventRouter() {
  input_method::InputMethodManager::GetInstance()->RemoveObserver(this);
}

void ExtensionInputMethodEventRouter::InputMethodChanged(
    input_method::InputMethodManager *manager,
    const input_method::InputMethodDescriptor &current_input_method,
    size_t num_active_input_methods) {
  Profile *profile = ProfileManager::GetDefaultProfile();
  ExtensionEventRouter *router = profile->GetExtensionEventRouter();

  if (!router->HasEventListener(extension_event_names::kOnInputMethodChanged))
    return;

  ListValue args;
  StringValue *input_method_name =
      new StringValue(GetInputMethodForXkb(current_input_method.id()));
  args.Append(input_method_name);
  std::string args_json;
  base::JSONWriter::Write(&args, false, &args_json);

  const std::vector<std::string>& ids =
      g_input_method_private_extensions_whitelist.Get().ids();

  for (size_t i = 0; i < ids.size(); ++i) {
    // ExtensionEventRoutner will check that the extension is listening for the
    // event.
    router->DispatchEventToExtension(
        ids[i], extension_event_names::kOnInputMethodChanged,
        args_json, profile, GURL());
  }
}

void ExtensionInputMethodEventRouter::ActiveInputMethodsChanged(
    input_method::InputMethodManager *manager,
    const input_method::InputMethodDescriptor & current_input_method,
    size_t num_active_input_methods) {
}

void ExtensionInputMethodEventRouter::PropertyListChanged(
    input_method::InputMethodManager *manager,
    const input_method::ImePropertyList & current_ime_properties) {
}

// static
std::string ExtensionInputMethodEventRouter::GetInputMethodForXkb(
    const std::string& xkb_id) {
  size_t prefix_length = std::string(kXkbPrefix).length();
  DCHECK(xkb_id.substr(0, prefix_length) == kXkbPrefix);
  return xkb_id.substr(prefix_length);
}

// static
bool ExtensionInputMethodEventRouter::IsExtensionWhitelisted(
    const std::string& extension_id) {
  return g_input_method_private_extensions_whitelist.Get().HasId(extension_id);
}

}  // namespace chromeos
