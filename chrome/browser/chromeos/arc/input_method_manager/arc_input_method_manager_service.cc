// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_service.h"

#include "base/logging.h"
#include "base/memory/singleton.h"
#include "chrome/browser/chromeos/arc/input_method_manager/arc_input_method_manager_bridge_impl.h"
#include "components/arc/arc_browser_context_keyed_service_factory_base.h"
#include "ui/base/ime/chromeos/extension_ime_util.h"

namespace arc {

namespace {

// The Android IME id of the pre-installed IME to proxy Chrome OS IME's actions
// to inside the container.
// Please refer to ArcImeService for the implementation details.
constexpr char kChromeOSIMEIdInArcContainer[] =
    "org.chromium.arc.ime/.ArcInputMethodService";

// Singleton factory for ArcInputMethodManagerService
class ArcInputMethodManagerServiceFactory
    : public internal::ArcBrowserContextKeyedServiceFactoryBase<
          ArcInputMethodManagerService,
          ArcInputMethodManagerServiceFactory> {
 public:
  // Factory name used by ArcBrowserContextKeyedServiceFactoryBase
  static constexpr const char* kName = "ArcInputMethodManagerServiceFactory";

  static ArcInputMethodManagerServiceFactory* GetInstance() {
    return base::Singleton<ArcInputMethodManagerServiceFactory>::get();
  }

 private:
  friend base::DefaultSingletonTraits<ArcInputMethodManagerServiceFactory>;
  ArcInputMethodManagerServiceFactory() = default;
  ~ArcInputMethodManagerServiceFactory() override = default;
};

}  // namespace

// static
ArcInputMethodManagerService*
ArcInputMethodManagerService::GetForBrowserContext(
    content::BrowserContext* context) {
  return ArcInputMethodManagerServiceFactory::GetForBrowserContext(context);
}

// static
ArcInputMethodManagerService*
ArcInputMethodManagerService::GetForBrowserContextForTesting(
    content::BrowserContext* context) {
  return ArcInputMethodManagerServiceFactory::GetForBrowserContextForTesting(
      context);
}

ArcInputMethodManagerService::ArcInputMethodManagerService(
    content::BrowserContext* context,
    ArcBridgeService* bridge_service)
    : imm_bridge_(
          std::make_unique<ArcInputMethodManagerBridgeImpl>(this,
                                                            bridge_service)) {
  auto* imm = chromeos::input_method::InputMethodManager::Get();
  imm->AddObserver(this);
  imm->AddImeMenuObserver(this);
}

ArcInputMethodManagerService::~ArcInputMethodManagerService() {
  auto* imm = chromeos::input_method::InputMethodManager::Get();
  imm->RemoveImeMenuObserver(this);
  imm->RemoveObserver(this);
}

void ArcInputMethodManagerService::SetInputMethodManagerBridgeForTesting(
    std::unique_ptr<ArcInputMethodManagerBridge> test_bridge) {
  imm_bridge_ = std::move(test_bridge);
}

void ArcInputMethodManagerService::OnActiveImeChanged(
    const std::string& ime_id) {
  // Please see https://crbug.com/845079.
  NOTIMPLEMENTED();
}

void ArcInputMethodManagerService::OnImeInfoChanged(
    std::vector<mojom::ImeInfoPtr> ime_info_array) {
  // Please see https://crbug.com/845079.
  NOTIMPLEMENTED();
}

void ArcInputMethodManagerService::ImeMenuListChanged() {
  auto* manager = chromeos::input_method::InputMethodManager::Get();
  auto new_active_ime_ids =
      manager->GetActiveIMEState()->GetActiveInputMethodIds();

  // Filter out non ARC IME ids.
  std::set<std::string> new_arc_active_ime_ids;
  std::copy_if(
      new_active_ime_ids.begin(), new_active_ime_ids.end(),
      std::inserter(new_arc_active_ime_ids, new_arc_active_ime_ids.end()),
      [](const auto& id) {
        return chromeos::extension_ime_util::IsArcIME(id);
      });

  for (const auto& id : new_arc_active_ime_ids) {
    // Enable the IME which is not currently enabled.
    if (!active_arc_ime_ids_.count(id))
      EnableIme(id, true /* enable */);
  }

  for (const auto& id : active_arc_ime_ids_) {
    // Disable the IME which is currently enabled.
    if (!new_arc_active_ime_ids.count(id))
      EnableIme(id, false /* enable */);
  }
  active_arc_ime_ids_.swap(new_arc_active_ime_ids);
}

void ArcInputMethodManagerService::InputMethodChanged(
    chromeos::input_method::InputMethodManager* manager,
    Profile* profile,
    bool /* show_message */) {
  auto state = manager->GetActiveIMEState();
  if (!state)
    return;
  SwitchImeTo(state->GetCurrentInputMethod().id());
}

void ArcInputMethodManagerService::EnableIme(const std::string& ime_id,
                                             bool enable) {
  auto component_id =
      chromeos::extension_ime_util::GetComponentIDByInputMethodID(ime_id);

  // TODO(yhanada): Disable the IME in Chrome OS side if it fails.
  imm_bridge_->SendEnableIme(
      component_id, enable,
      base::BindOnce(
          [](const std::string& ime_id, bool enable, bool success) {
            if (!success) {
              LOG(ERROR) << (enable ? "Enabling" : "Disabling") << " \""
                         << ime_id << "\" failed";
            }
          },
          ime_id, enable));
}

void ArcInputMethodManagerService::SwitchImeTo(const std::string& ime_id) {
  using namespace chromeos::extension_ime_util;

  std::string component_id = GetComponentIDByInputMethodID(ime_id);
  if (!IsArcIME(ime_id))
    component_id = kChromeOSIMEIdInArcContainer;
  imm_bridge_->SendSwitchImeTo(
      component_id, base::BindOnce(
                        [](const std::string& ime_id,
                           const std::string& component_id, bool success) {
                          if (!success) {
                            LOG(ERROR) << "Switch the active IME to \""
                                       << ime_id << "\"(component_id=\""
                                       << component_id << "\") failed";
                          }
                        },
                        ime_id, component_id));
}

}  // namespace arc
