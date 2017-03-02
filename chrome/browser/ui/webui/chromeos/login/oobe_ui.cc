// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

#include <stddef.h>

#include <memory>

#include "ash/common/wm/screen_dimmer.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/chromeos/login/enrollment/auto_enrollment_check_screen_view.h"
#include "chrome/browser/chromeos/login/enrollment/enrollment_screen_view.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/policy/browser_policy_connector_chromeos.h"
#include "chrome/browser/chromeos/settings/shutdown_policy_handler.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/extensions/signin/gaia_auth_extension_loader.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/webui/about_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_kiosk_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/arc_terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/auto_enrollment_check_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/controller_pairing_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enable_debugging_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/enrollment_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/eula_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/hid_detection_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/host_pairing_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_app_menu_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_autolaunch_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/kiosk_enable_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_dropdown_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/reset_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/supervised_user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/terms_of_service_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_board_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/user_image_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/wrong_hwid_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/network_element_localized_strings_provider.h"
#include "chrome/browser/ui/webui/options/chromeos/user_image_source.h"
#include "chrome/browser/ui/webui/test_files_request_filter.h"
#include "chrome/browser/ui/webui/theme_source.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/chrome_unscaled_resources.h"
#include "chrome/grit/component_extension_resources.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/content_switches.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/webui/web_ui_util.h"

namespace chromeos {

namespace {

const char* kKnownDisplayTypes[] = {OobeUI::kOobeDisplay,
                                    OobeUI::kLoginDisplay,
                                    OobeUI::kLockDisplay,
                                    OobeUI::kUserAddingDisplay,
                                    OobeUI::kAppLaunchSplashDisplay,
                                    OobeUI::kArcKioskSplashDisplay};

OobeScreen kDimOverlayScreenIds[] = {
  OobeScreen::SCREEN_CONFIRM_PASSWORD,
  OobeScreen::SCREEN_GAIA_SIGNIN,
  OobeScreen::SCREEN_OOBE_ENROLLMENT,
  OobeScreen::SCREEN_PASSWORD_CHANGED,
  OobeScreen::SCREEN_USER_IMAGE_PICKER
};

const char kStringsJSPath[] = "strings.js";
const char kLockJSPath[] = "lock.js";
const char kLoginJSPath[] = "login.js";
const char kOobeJSPath[] = "oobe.js";
const char kKeyboardUtilsJSPath[] = "keyboard_utils.js";
const char kCustomElementsHTMLPath[] = "custom_elements.html";
const char kCustomElementsJSPath[] = "custom_elements.js";
const char kCustomElementsUserPodHTMLPath[] = "custom_elements_user_pod.html";

// Paths for deferred resource loading.
const char kCustomElementsPinKeyboardHTMLPath[] =
    "custom_elements/pin_keyboard.html";
const char kCustomElementsPinKeyboardJSPath[] =
    "custom_elements/pin_keyboard.js";
const char kEnrollmentHTMLPath[] = "enrollment.html";
const char kEnrollmentCSSPath[] = "enrollment.css";
const char kEnrollmentJSPath[] = "enrollment.js";
const char kArcPlaystoreCSSPath[] = "playstore.css";
const char kArcPlaystoreJSPath[] = "playstore.js";
const char kArcPlaystoreLogoPath[] = "playstore.svg";

// Creates a WebUIDataSource for chrome://oobe
content::WebUIDataSource* CreateOobeUIDataSource(
    const base::DictionaryValue& localized_strings,
    const std::string& display_type) {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIOobeHost);
  source->AddLocalizedStrings(localized_strings);
  source->SetJsonPath(kStringsJSPath);

  if (display_type == OobeUI::kOobeDisplay) {
    source->SetDefaultResource(IDR_OOBE_HTML);
    source->AddResourcePath(kOobeJSPath, IDR_OOBE_JS);
    source->AddResourcePath(kCustomElementsHTMLPath,
                            IDR_CUSTOM_ELEMENTS_OOBE_HTML);
    source->AddResourcePath(kCustomElementsJSPath, IDR_CUSTOM_ELEMENTS_OOBE_JS);
  } else if (display_type == OobeUI::kLockDisplay) {
    source->SetDefaultResource(IDR_LOCK_HTML);
    source->AddResourcePath(kLockJSPath, IDR_LOCK_JS);
    source->AddResourcePath(kCustomElementsHTMLPath,
                            IDR_CUSTOM_ELEMENTS_LOCK_HTML);
    source->AddResourcePath(kCustomElementsJSPath, IDR_CUSTOM_ELEMENTS_LOCK_JS);
    source->AddResourcePath(kCustomElementsPinKeyboardHTMLPath,
                            IDR_CUSTOM_ELEMENTS_PIN_KEYBOARD_HTML);
    source->AddResourcePath(kCustomElementsPinKeyboardJSPath,
                            IDR_CUSTOM_ELEMENTS_PIN_KEYBOARD_JS);
    source->AddResourcePath(kCustomElementsUserPodHTMLPath,
                            IDR_CUSTOM_ELEMENTS_USER_POD_HTML);
  } else {
    source->SetDefaultResource(IDR_LOGIN_HTML);
    source->AddResourcePath(kLoginJSPath, IDR_LOGIN_JS);
    source->AddResourcePath(kCustomElementsHTMLPath,
                            IDR_CUSTOM_ELEMENTS_LOGIN_HTML);
    source->AddResourcePath(kCustomElementsJSPath,
                            IDR_CUSTOM_ELEMENTS_LOGIN_JS);
    source->AddResourcePath(kCustomElementsUserPodHTMLPath,
                            IDR_CUSTOM_ELEMENTS_USER_POD_HTML);
  }

  // Required for postprocessing of Goolge PlayStore Terms.
  source->AddResourcePath(kArcPlaystoreCSSPath, IDR_ARC_SUPPORT_PLAYSTORE_CSS);
  source->AddResourcePath(kArcPlaystoreJSPath, IDR_ARC_SUPPORT_PLAYSTORE_JS);
  source->AddResourcePath(kArcPlaystoreLogoPath,
      IDR_ARC_SUPPORT_PLAYSTORE_LOGO);

  source->AddResourcePath(kKeyboardUtilsJSPath, IDR_KEYBOARD_UTILS_JS);
  source->OverrideContentSecurityPolicyChildSrc(
      base::StringPrintf(
          "child-src chrome://terms/ %s/;",
          extensions::kGaiaAuthExtensionOrigin));
  source->OverrideContentSecurityPolicyObjectSrc(
      "object-src chrome:;");

  // Serve deferred resources.
  source->AddResourcePath(kEnrollmentHTMLPath, IDR_OOBE_ENROLLMENT_HTML);
  source->AddResourcePath(kEnrollmentCSSPath, IDR_OOBE_ENROLLMENT_CSS);
  source->AddResourcePath(kEnrollmentJSPath, IDR_OOBE_ENROLLMENT_JS);

  // Only add a filter when runing as test.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const bool is_running_test = command_line->HasSwitch(::switches::kTestName) ||
                               command_line->HasSwitch(::switches::kTestType);
  if (is_running_test)
    source->SetRequestFilter(::test::GetTestFilesRequestFilter());

  return source;
}

std::string GetDisplayType(const GURL& url) {
  std::string path = url.path().size() ? url.path().substr(1) : "";
  if (std::find(kKnownDisplayTypes,
                kKnownDisplayTypes + arraysize(kKnownDisplayTypes),
                path) == kKnownDisplayTypes + arraysize(kKnownDisplayTypes)) {
    LOG(ERROR) << "Unknown display type '" << path << "'. Setting default.";
    return OobeUI::kLoginDisplay;
  }
  return path;
}

}  // namespace

// static
const char OobeUI::kOobeDisplay[] = "oobe";
const char OobeUI::kLoginDisplay[] = "login";
const char OobeUI::kLockDisplay[] = "lock";
const char OobeUI::kUserAddingDisplay[] = "user-adding";
const char OobeUI::kAppLaunchSplashDisplay[] = "app-launch-splash";
const char OobeUI::kArcKioskSplashDisplay[] = "arc-kiosk-splash";

OobeUI::OobeUI(content::WebUI* web_ui, const GURL& url)
    : WebUIController(web_ui) {
  display_type_ = GetDisplayType(url);

  network_state_informer_ = new NetworkStateInformer();
  network_state_informer_->Init();

  js_calls_container = base::MakeUnique<JSCallsContainer>();

  auto core_handler =
      base::MakeUnique<CoreOobeHandler>(this, js_calls_container.get());
  core_handler_ = core_handler.get();
  AddScreenHandler(std::move(core_handler));
  core_handler_->SetDelegate(this);

  auto network_dropdown_handler = base::MakeUnique<NetworkDropdownHandler>();
  network_dropdown_handler_ = network_dropdown_handler.get();
  AddScreenHandler(std::move(network_dropdown_handler));

  auto update_screen_handler = base::MakeUnique<UpdateScreenHandler>();
  update_view_ = update_screen_handler.get();
  AddScreenHandler(std::move(update_screen_handler));

  if (display_type_ == kOobeDisplay) {
    auto network_screen_handler =
        base::MakeUnique<NetworkScreenHandler>(core_handler_);
    network_view_ = network_screen_handler.get();
    AddScreenHandler(std::move(network_screen_handler));
  }

  auto debugging_screen_handler =
      base::MakeUnique<EnableDebuggingScreenHandler>();
  debugging_screen_view_ = debugging_screen_handler.get();
  AddScreenHandler(std::move(debugging_screen_handler));

  auto eula_screen_handler = base::MakeUnique<EulaScreenHandler>(core_handler_);
  eula_view_ = eula_screen_handler.get();
  AddScreenHandler(std::move(eula_screen_handler));

  auto reset_screen_handler = base::MakeUnique<ResetScreenHandler>();
  reset_view_ = reset_screen_handler.get();
  AddScreenHandler(std::move(reset_screen_handler));

  auto autolaunch_screen_handler =
      base::MakeUnique<KioskAutolaunchScreenHandler>();
  autolaunch_screen_view_ = autolaunch_screen_handler.get();
  AddScreenHandler(std::move(autolaunch_screen_handler));

  auto kiosk_enable_screen_handler =
      base::MakeUnique<KioskEnableScreenHandler>();
  kiosk_enable_screen_view_ = kiosk_enable_screen_handler.get();
  AddScreenHandler(std::move(kiosk_enable_screen_handler));

  auto supervised_user_creation_screen_handler =
      base::MakeUnique<SupervisedUserCreationScreenHandler>();
  supervised_user_creation_screen_view_ =
      supervised_user_creation_screen_handler.get();
  AddScreenHandler(std::move(supervised_user_creation_screen_handler));

  auto wrong_hwid_screen_handler = base::MakeUnique<WrongHWIDScreenHandler>();
  wrong_hwid_screen_view_ = wrong_hwid_screen_handler.get();
  AddScreenHandler(std::move(wrong_hwid_screen_handler));

  auto auto_enrollment_check_screen_handler =
      base::MakeUnique<AutoEnrollmentCheckScreenHandler>();
  auto_enrollment_check_screen_view_ =
      auto_enrollment_check_screen_handler.get();
  AddScreenHandler(std::move(auto_enrollment_check_screen_handler));

  auto hid_detection_screen_handler =
      base::MakeUnique<HIDDetectionScreenHandler>(core_handler_);
  hid_detection_view_ = hid_detection_screen_handler.get();
  AddScreenHandler(std::move(hid_detection_screen_handler));

  auto error_screen_handler = base::MakeUnique<ErrorScreenHandler>();
  error_screen_handler_ = error_screen_handler.get();
  AddScreenHandler(std::move(error_screen_handler));
  network_dropdown_handler_->AddObserver(error_screen_handler_);

  error_screen_.reset(new ErrorScreen(nullptr, error_screen_handler_));
  ErrorScreen* error_screen = error_screen_.get();

  auto enrollment_screen_handler = base::MakeUnique<EnrollmentScreenHandler>(
      network_state_informer_, error_screen);
  enrollment_screen_view_ = enrollment_screen_handler.get();
  AddScreenHandler(std::move(enrollment_screen_handler));

  auto terms_of_service_screen_handler =
      base::MakeUnique<TermsOfServiceScreenHandler>(core_handler_);
  terms_of_service_screen_view_ = terms_of_service_screen_handler.get();
  AddScreenHandler(std::move(terms_of_service_screen_handler));

  auto arc_terms_of_service_screen_handler =
      base::MakeUnique<ArcTermsOfServiceScreenHandler>();
  arc_terms_of_service_screen_view_ = arc_terms_of_service_screen_handler.get();
  AddScreenHandler(std::move(arc_terms_of_service_screen_handler));

  auto user_image_screen_handler = base::MakeUnique<UserImageScreenHandler>();
  user_image_view_ = user_image_screen_handler.get();
  AddScreenHandler(std::move(user_image_screen_handler));

  auto user_board_screen_handler = base::MakeUnique<UserBoardScreenHandler>();
  user_board_screen_handler_ = user_board_screen_handler.get();
  AddScreenHandler(std::move(user_board_screen_handler));

  auto gaia_screen_handler = base::MakeUnique<GaiaScreenHandler>(
      core_handler_, network_state_informer_);
  gaia_screen_handler_ = gaia_screen_handler.get();
  AddScreenHandler(std::move(gaia_screen_handler));

  auto signin_screen_handler = base::MakeUnique<SigninScreenHandler>(
      network_state_informer_, error_screen, core_handler_,
      gaia_screen_handler_, js_calls_container.get());
  signin_screen_handler_ = signin_screen_handler.get();
  AddScreenHandler(std::move(signin_screen_handler));

  auto app_launch_splash_screen_handler =
      base::MakeUnique<AppLaunchSplashScreenHandler>(network_state_informer_,
                                                     error_screen);
  app_launch_splash_screen_view_ = app_launch_splash_screen_handler.get();
  AddScreenHandler(std::move(app_launch_splash_screen_handler));

  auto arc_kiosk_splash_screen_handler =
      base::MakeUnique<ArcKioskSplashScreenHandler>();
  arc_kiosk_splash_screen_view_ = arc_kiosk_splash_screen_handler.get();
  AddScreenHandler(std::move(arc_kiosk_splash_screen_handler));

  if (display_type_ == kOobeDisplay) {
    auto controller_pairing_handler =
        base::MakeUnique<ControllerPairingScreenHandler>();
    controller_pairing_screen_view_ = controller_pairing_handler.get();
    AddScreenHandler(std::move(controller_pairing_handler));

    auto host_pairing_handler = base::MakeUnique<HostPairingScreenHandler>();
    host_pairing_screen_view_ = host_pairing_handler.get();
    AddScreenHandler(std::move(host_pairing_handler));
  }

  auto device_disabled_screen_handler =
      base::MakeUnique<DeviceDisabledScreenHandler>();
  device_disabled_screen_view_ = device_disabled_screen_handler.get();
  AddScreenHandler(std::move(device_disabled_screen_handler));

  // Initialize KioskAppMenuHandler. Note that it is NOT a screen handler.
  auto kiosk_app_menu_handler =
      base::MakeUnique<KioskAppMenuHandler>(network_state_informer_);
  kiosk_app_menu_handler_ = kiosk_app_menu_handler.get();
  web_ui->AddMessageHandler(std::move(kiosk_app_menu_handler));

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);

  Profile* profile = Profile::FromWebUI(web_ui);
  // Set up the chrome://theme/ source, for Chrome logo.
  ThemeSource* theme = new ThemeSource(profile);
  content::URLDataSource::Add(profile, theme);

  // Set up the chrome://terms/ data source, for EULA content.
  AboutUIHTMLSource* about_source =
      new AboutUIHTMLSource(chrome::kChromeUITermsHost, profile);
  content::URLDataSource::Add(profile, about_source);

  // Set up the chrome://oobe/ source.
  content::WebUIDataSource* html_source =
      CreateOobeUIDataSource(localized_strings, display_type_);
  content::WebUIDataSource::Add(profile, html_source);
  network_element::AddLocalizedStrings(html_source);

  // Set up the chrome://userimage/ source.
  options::UserImageSource* user_image_source =
      new options::UserImageSource();
  content::URLDataSource::Add(profile, user_image_source);

  // TabHelper is required for OOBE webui to make webview working on it.
  content::WebContents* contents = web_ui->GetWebContents();
  extensions::TabHelper::CreateForWebContents(contents);
}

OobeUI::~OobeUI() {
  core_handler_->SetDelegate(nullptr);
  network_dropdown_handler_->RemoveObserver(error_screen_handler_);
  if (ash_util::IsRunningInMash()) {
    // TODO: Ash needs to expose screen dimming api. See
    // http://crbug.com/646034.
    NOTIMPLEMENTED();
  }
}

CoreOobeView* OobeUI::GetCoreOobeView() {
  return core_handler_;
}

NetworkView* OobeUI::GetNetworkView() {
  return network_view_;
}

EulaView* OobeUI::GetEulaView() {
  return eula_view_;
}

UpdateView* OobeUI::GetUpdateView() {
  return update_view_;
}

EnableDebuggingScreenView* OobeUI::GetEnableDebuggingScreenView() {
  return debugging_screen_view_;
}

EnrollmentScreenView* OobeUI::GetEnrollmentScreenView() {
  return enrollment_screen_view_;
}

ResetView* OobeUI::GetResetView() {
  return reset_view_;
}

KioskAutolaunchScreenView* OobeUI::GetKioskAutolaunchScreenView() {
  return autolaunch_screen_view_;
}

KioskEnableScreenView* OobeUI::GetKioskEnableScreenView() {
  return kiosk_enable_screen_view_;
}

TermsOfServiceScreenView* OobeUI::GetTermsOfServiceScreenView() {
  return terms_of_service_screen_view_;
}

ArcTermsOfServiceScreenView* OobeUI::GetArcTermsOfServiceScreenView() {
  return arc_terms_of_service_screen_view_;
}

WrongHWIDScreenView* OobeUI::GetWrongHWIDScreenView() {
  return wrong_hwid_screen_view_;
}

AutoEnrollmentCheckScreenView* OobeUI::GetAutoEnrollmentCheckScreenView() {
  return auto_enrollment_check_screen_view_;
}

HIDDetectionView* OobeUI::GetHIDDetectionView() {
  return hid_detection_view_;
}

ControllerPairingScreenView* OobeUI::GetControllerPairingScreenView() {
  return controller_pairing_screen_view_;
}

HostPairingScreenView* OobeUI::GetHostPairingScreenView() {
  return host_pairing_screen_view_;
}

DeviceDisabledScreenView* OobeUI::GetDeviceDisabledScreenView() {
  return device_disabled_screen_view_;
}

UserImageView* OobeUI::GetUserImageView() {
  return user_image_view_;
}

ErrorScreen* OobeUI::GetErrorScreen() {
  return error_screen_.get();
}

SupervisedUserCreationScreenHandler*
OobeUI::GetSupervisedUserCreationScreenView() {
  return supervised_user_creation_screen_view_;
}

GaiaView* OobeUI::GetGaiaScreenView() {
  return gaia_screen_handler_;
}

UserBoardView* OobeUI::GetUserBoardView() {
  return user_board_screen_handler_;
}

void OobeUI::OnShutdownPolicyChanged(bool reboot_on_shutdown) {
  core_handler_->UpdateShutdownAndRebootVisibility(reboot_on_shutdown);
}

AppLaunchSplashScreenView* OobeUI::GetAppLaunchSplashScreenView() {
  return app_launch_splash_screen_view_;
}

ArcKioskSplashScreenView* OobeUI::GetArcKioskSplashScreenView() {
  return arc_kiosk_splash_screen_view_;
}

void OobeUI::GetLocalizedStrings(base::DictionaryValue* localized_strings) {
  // Note, handlers_[0] is a GenericHandler used by the WebUI.
  for (size_t i = 0; i < handlers_.size(); ++i) {
    handlers_[i]->GetLocalizedStrings(localized_strings);
  }
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings);
  kiosk_app_menu_handler_->GetLocalizedStrings(localized_strings);

#if defined(GOOGLE_CHROME_BUILD)
  localized_strings->SetString("buildType", "chrome");
#else
  localized_strings->SetString("buildType", "chromium");
#endif

  // If we're not doing boot animation then WebUI should trigger
  // wallpaper load on boot.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableBootAnimation)) {
    localized_strings->SetString("bootIntoWallpaper", "on");
  } else {
    localized_strings->SetString("bootIntoWallpaper", "off");
  }

  bool keyboard_driven_oobe =
      system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation();
  localized_strings->SetString("highlightStrength",
                               keyboard_driven_oobe ? "strong" : "normal");

  bool new_kiosk_ui = KioskAppMenuHandler::EnableNewKioskUI();
  localized_strings->SetString("newKioskUI", new_kiosk_ui ? "on" : "off");
  oobe_ui_md_mode_ =
      g_browser_process->local_state()->GetBoolean(prefs::kOobeMdMode);
  localized_strings->SetString("newOobeUI", oobe_ui_md_mode_ ? "on" : "off");
}

void OobeUI::AddScreenHandler(std::unique_ptr<BaseScreenHandler> handler) {
  handlers_.push_back(handler.get());
  web_ui()->AddMessageHandler(std::move(handler));
}

void OobeUI::InitializeHandlers() {
  ready_ = true;
  for (size_t i = 0; i < ready_callbacks_.size(); ++i)
    ready_callbacks_[i].Run();
  ready_callbacks_.clear();

  // Notify 'initialize' for synchronously loaded screens.
  for (size_t i = 0; i < handlers_.size(); ++i) {
    if (handlers_[i]->async_assets_load_id().empty())
      handlers_[i]->InitializeBase();
  }

  // Instantiate the ShutdownPolicyHandler.
  shutdown_policy_handler_.reset(
      new ShutdownPolicyHandler(CrosSettings::Get(), this));

  // Trigger an initial update.
  shutdown_policy_handler_->NotifyDelegateWithShutdownPolicy();
}

void OobeUI::OnScreenAssetsLoaded(const std::string& async_assets_load_id) {
  DCHECK(!async_assets_load_id.empty());

  for (size_t i = 0; i < handlers_.size(); ++i) {
    if (handlers_[i]->async_assets_load_id() == async_assets_load_id)
      handlers_[i]->InitializeBase();
  }
}

bool OobeUI::IsJSReady(const base::Closure& display_is_ready_callback) {
  if (!ready_)
    ready_callbacks_.push_back(display_is_ready_callback);
  return ready_;
}

void OobeUI::ShowOobeUI(bool show) {
  core_handler_->ShowOobeUI(show);
}

void OobeUI::ShowSigninScreen(const LoginScreenContext& context,
                              SigninScreenHandlerDelegate* delegate,
                              NativeWindowDelegate* native_window_delegate) {
  // Check our device mode.
  policy::BrowserPolicyConnectorChromeOS* connector =
      g_browser_process->platform_part()->browser_policy_connector_chromeos();
  if (connector->GetDeviceMode() == policy::DEVICE_MODE_LEGACY_RETAIL_MODE) {
    // If we're in legacy retail mode, the best thing we can do is launch the
    // new offline demo mode.
    LoginDisplayHost::default_host()->StartDemoAppLaunch();
    return;
  }

  signin_screen_handler_->SetDelegate(delegate);
  signin_screen_handler_->SetNativeWindowDelegate(native_window_delegate);

  LoginScreenContext actual_context(context);
  actual_context.set_oobe_ui(core_handler_->show_oobe_ui());
  signin_screen_handler_->Show(actual_context);
}

void OobeUI::ResetSigninScreenHandlerDelegate() {
  signin_screen_handler_->SetDelegate(nullptr);
  signin_screen_handler_->SetNativeWindowDelegate(nullptr);
}


void OobeUI::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void OobeUI::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void OobeUI::OnCurrentScreenChanged(OobeScreen new_screen) {
  previous_screen_ = current_screen_;

  const bool should_dim =
      std::find(std::begin(kDimOverlayScreenIds),
                std::end(kDimOverlayScreenIds),
                new_screen) != std::end(kDimOverlayScreenIds);
  if (!ash_util::IsRunningInMash()) {
    if (!screen_dimmer_) {
      screen_dimmer_ = base::MakeUnique<ash::ScreenDimmer>(
          ash::ScreenDimmer::Container::LOCK_SCREEN);
    }
    screen_dimmer_->set_at_bottom(true);
    screen_dimmer_->SetDimming(should_dim);
  } else {
    // TODO: Ash needs to expose screen dimming api. See
    // http://crbug.com/646034.
    NOTIMPLEMENTED();
  }

  current_screen_ = new_screen;
  for (Observer& observer : observer_list_)
    observer.OnCurrentScreenChanged(current_screen_, new_screen);
}

void OobeUI::UpdateLocalizedStringsIfNeeded() {
  if (oobe_ui_md_mode_ ==
      g_browser_process->local_state()->GetBoolean(prefs::kOobeMdMode)) {
    return;
  }

  base::DictionaryValue localized_strings;
  GetLocalizedStrings(&localized_strings);
  static_cast<CoreOobeView*>(core_handler_)->ReloadContent(localized_strings);
}

}  // namespace chromeos
