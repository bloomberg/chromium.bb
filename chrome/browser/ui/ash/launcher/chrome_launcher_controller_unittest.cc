// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"

#include <algorithm>
#include <string>
#include <vector>

#include "ash/ash_switches.h"
#include "ash/shelf/shelf_item_delegate_manager.h"
#include "ash/shelf/shelf_model.h"
#include "ash/shelf/shelf_model_observer.h"
#include "ash/shell.h"
#include "ash/test/shelf_item_delegate_manager_test_api.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/ui/ash/chrome_launcher_prefs.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/launcher_application_menu_item_model.h"
#include "chrome/browser/ui/ash/launcher/launcher_item_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/base/models/menu_model.h"

#if defined(OS_CHROMEOS)
#include "apps/app_window_contents.h"
#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "ash/test/test_session_state_delegate.h"
#include "ash/test/test_shell_delegate.h"
#include "chrome/browser/chromeos/login/users/fake_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/browser_status_monitor.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_util.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_chromeos.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/test_utils.h"
#include "ui/aura/window.h"
#endif

using base::ASCIIToUTF16;
using extensions::Extension;
using extensions::Manifest;
using extensions::UnloadedExtensionInfo;

namespace {
const char* offline_gmail_url = "https://mail.google.com/mail/mu/u";
const char* gmail_url = "https://mail.google.com/mail/u";
const char* kGmailLaunchURL = "https://mail.google.com/mail/ca";

#if defined(OS_CHROMEOS)
// As defined in /chromeos/dbus/cryptohome_client.cc.
const char kUserIdHashSuffix[] = "-hash";

// An extension prefix.
const char kCrxAppPrefix[] = "_crx_";
#endif

// ShelfModelObserver implementation that tracks what messages are invoked.
class TestShelfModelObserver : public ash::ShelfModelObserver {
 public:
  TestShelfModelObserver()
    : added_(0),
      removed_(0),
      changed_(0) {
  }

  virtual ~TestShelfModelObserver() {
  }

  // Overridden from ash::ShelfModelObserver:
  virtual void ShelfItemAdded(int index) OVERRIDE {
    ++added_;
    last_index_ = index;
  }

  virtual void ShelfItemRemoved(int index, ash::ShelfID id) OVERRIDE {
    ++removed_;
    last_index_ = index;
  }

  virtual void ShelfItemChanged(int index,
                                const ash::ShelfItem& old_item) OVERRIDE {
    ++changed_;
    last_index_ = index;
  }

  virtual void ShelfItemMoved(int start_index, int target_index) OVERRIDE {
    last_index_ = target_index;
  }

  virtual void ShelfStatusChanged() OVERRIDE {
  }

  void clear_counts() {
    added_ = 0;
    removed_ = 0;
    changed_ = 0;
    last_index_ = 0;
  }

  int added() const { return added_; }
  int removed() const { return removed_; }
  int changed() const { return changed_; }
  int last_index() const { return last_index_; }

 private:
  int added_;
  int removed_;
  int changed_;
  int last_index_;

  DISALLOW_COPY_AND_ASSIGN(TestShelfModelObserver);
};

// Test implementation of AppIconLoader.
class TestAppIconLoaderImpl : public extensions::AppIconLoader {
 public:
  TestAppIconLoaderImpl() : fetch_count_(0) {
  }

  virtual ~TestAppIconLoaderImpl() {
  }

  // AppIconLoader implementation:
  virtual void FetchImage(const std::string& id) OVERRIDE {
    ++fetch_count_;
  }

  virtual void ClearImage(const std::string& id) OVERRIDE {
  }

  virtual void UpdateImage(const std::string& id) OVERRIDE {
  }

  int fetch_count() const { return fetch_count_; }

 private:
  int fetch_count_;

  DISALLOW_COPY_AND_ASSIGN(TestAppIconLoaderImpl);
};

// Test implementation of AppTabHelper.
class TestAppTabHelperImpl : public ChromeLauncherController::AppTabHelper {
 public:
  TestAppTabHelperImpl() {}
  virtual ~TestAppTabHelperImpl() {}

  // Sets the id for the specified tab. The id is removed if Remove() is
  // invoked.
  void SetAppID(content::WebContents* tab, const std::string& id) {
    tab_id_map_[tab] = id;
  }

  // Returns true if there is an id registered for |tab|.
  bool HasAppID(content::WebContents* tab) const {
    return tab_id_map_.find(tab) != tab_id_map_.end();
  }

  // AppTabHelper implementation:
  virtual std::string GetAppID(content::WebContents* tab) OVERRIDE {
    return tab_id_map_.find(tab) != tab_id_map_.end() ? tab_id_map_[tab] :
        std::string();
  }

  virtual bool IsValidIDForCurrentUser(const std::string& id) OVERRIDE {
    for (TabToStringMap::const_iterator i = tab_id_map_.begin();
         i != tab_id_map_.end(); ++i) {
      if (i->second == id)
        return true;
    }
    return false;
  }

  virtual void SetCurrentUser(Profile* profile) OVERRIDE {
    // We can ignore this for now.
  }

 private:
  typedef std::map<content::WebContents*, std::string> TabToStringMap;

  TabToStringMap tab_id_map_;

  DISALLOW_COPY_AND_ASSIGN(TestAppTabHelperImpl);
};

// Test implementation of a V2 app launcher item controller.
class TestV2AppLauncherItemController : public LauncherItemController {
 public:
  TestV2AppLauncherItemController(const std::string& app_id,
                                  ChromeLauncherController* controller)
      : LauncherItemController(LauncherItemController::TYPE_APP,
                               app_id,
                               controller) {
  }

  virtual ~TestV2AppLauncherItemController() {}

  // Override for LauncherItemController:
  virtual bool IsOpen() const OVERRIDE { return true; }
  virtual bool IsVisible() const OVERRIDE { return true; }
  virtual void Launch(ash::LaunchSource source, int event_flags) OVERRIDE {}
  virtual bool Activate(ash::LaunchSource source) OVERRIDE { return false; }
  virtual void Close() OVERRIDE {}
  virtual bool ItemSelected(const ui::Event& event) OVERRIDE { return false; }
  virtual base::string16 GetTitle() OVERRIDE { return base::string16(); }
  virtual ChromeLauncherAppMenuItems GetApplicationList(
      int event_flags) OVERRIDE {
    ChromeLauncherAppMenuItems items;
    items.push_back(
        new ChromeLauncherAppMenuItem(base::string16(), NULL, false));
    items.push_back(
        new ChromeLauncherAppMenuItem(base::string16(), NULL, false));
    return items.Pass();
  }
  virtual ui::MenuModel* CreateContextMenu(aura::Window* root_window) OVERRIDE {
    return NULL;
  }
  virtual ash::ShelfMenuModel* CreateApplicationMenu(int event_flags) OVERRIDE {
    return NULL;
  }
  virtual bool IsDraggable() OVERRIDE { return false; }
  virtual bool ShouldShowTooltip() OVERRIDE { return false; }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestV2AppLauncherItemController);
};

}  // namespace

class ChromeLauncherControllerTest : public BrowserWithTestWindowTest {
 protected:
  ChromeLauncherControllerTest()
      : BrowserWithTestWindowTest(
            Browser::TYPE_TABBED,
            chrome::HOST_DESKTOP_TYPE_ASH,
            false),
        test_controller_(NULL),
        extension_service_(NULL) {
  }

  virtual ~ChromeLauncherControllerTest() {
  }

  virtual void SetUp() OVERRIDE {
    BrowserWithTestWindowTest::SetUp();

    model_.reset(new ash::ShelfModel);
    model_observer_.reset(new TestShelfModelObserver);
    model_->AddObserver(model_observer_.get());

    if (ash::Shell::HasInstance()) {
      item_delegate_manager_ =
          ash::Shell::GetInstance()->shelf_item_delegate_manager();
    } else {
      item_delegate_manager_ =
          new ash::ShelfItemDelegateManager(model_.get());
    }

    base::DictionaryValue manifest;
    manifest.SetString(extensions::manifest_keys::kName,
                       "launcher controller test extension");
    manifest.SetString(extensions::manifest_keys::kVersion, "1");
    manifest.SetString(extensions::manifest_keys::kDescription,
                       "for testing pinned apps");

    extensions::TestExtensionSystem* extension_system(
        static_cast<extensions::TestExtensionSystem*>(
            extensions::ExtensionSystem::Get(profile())));
    extension_service_ = extension_system->CreateExtensionService(
        CommandLine::ForCurrentProcess(), base::FilePath(), false);

    std::string error;
    extension1_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                    &error);
    extension2_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                                    &error);
    // Fake gmail extension.
    base::DictionaryValue manifest_gmail;
    manifest_gmail.SetString(extensions::manifest_keys::kName,
                             "Gmail launcher controller test extension");
    manifest_gmail.SetString(extensions::manifest_keys::kVersion, "1");
    manifest_gmail.SetString(extensions::manifest_keys::kDescription,
                             "for testing pinned Gmail");
    manifest_gmail.SetString(extensions::manifest_keys::kLaunchWebURL,
                             kGmailLaunchURL);
    base::ListValue* list = new base::ListValue();
    list->Append(new base::StringValue("*://mail.google.com/mail/ca"));
    manifest_gmail.Set(extensions::manifest_keys::kWebURLs, list);

    extension3_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest_gmail,
                                    Extension::NO_FLAGS,
                                    extension_misc::kGmailAppId,
                                    &error);

    // Fake search extension.
    extension4_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    extension_misc::kGoogleSearchAppId,
                                    &error);
    extension5_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    "cccccccccccccccccccccccccccccccc",
                                    &error);
    extension6_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    "dddddddddddddddddddddddddddddddd",
                                    &error);
    extension7_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    "eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee",
                                    &error);
    extension8_ = Extension::Create(base::FilePath(), Manifest::UNPACKED,
                                    manifest,
                                    Extension::NO_FLAGS,
                                    "ffffffffffffffffffffffffffffffff",
                                    &error);
  }

  // Creates a running V2 app (not pinned) of type |app_id|.
  virtual void CreateRunningV2App(const std::string& app_id) {
    DCHECK(!test_controller_);
    ash::ShelfID id =
        launcher_controller_->CreateAppShortcutLauncherItemWithType(
            app_id,
            model_->item_count(),
            ash::TYPE_PLATFORM_APP);
    DCHECK(id);
    // Change the created launcher controller into a V2 app controller.
    test_controller_ = new TestV2AppLauncherItemController(app_id,
        launcher_controller_.get());
    launcher_controller_->SetItemController(id, test_controller_);
  }

  // Sets the stage for a multi user test.
  virtual void SetUpMultiUserScenario(base::ListValue* user_a,
                                      base::ListValue* user_b) {
    InitLauncherController();
    EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

    // Set an empty pinned pref to begin with.
    base::ListValue no_user;
    SetShelfChromeIconIndex(0);
    profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                    no_user.DeepCopy());
    EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

    // Assume all applications have been added already.
    extension_service_->AddExtension(extension1_.get());
    extension_service_->AddExtension(extension2_.get());
    extension_service_->AddExtension(extension3_.get());
    extension_service_->AddExtension(extension4_.get());
    extension_service_->AddExtension(extension5_.get());
    extension_service_->AddExtension(extension6_.get());
    extension_service_->AddExtension(extension7_.get());
    extension_service_->AddExtension(extension8_.get());
    // There should be nothing in the list by now.
    EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

    // Set user a preferences.
    InsertPrefValue(user_a, 0, extension1_->id());
    InsertPrefValue(user_a, 1, extension2_->id());
    InsertPrefValue(user_a, 2, extension3_->id());
    InsertPrefValue(user_a, 3, extension4_->id());
    InsertPrefValue(user_a, 4, extension5_->id());
    InsertPrefValue(user_a, 5, extension6_->id());

    // Set user b preferences.
    InsertPrefValue(user_b, 0, extension7_->id());
    InsertPrefValue(user_b, 1, extension8_->id());
  }

  virtual void TearDown() OVERRIDE {
    if (!ash::Shell::HasInstance())
      delete item_delegate_manager_;
    model_->RemoveObserver(model_observer_.get());
    model_observer_.reset();
    launcher_controller_.reset();
    model_.reset();

    BrowserWithTestWindowTest::TearDown();
  }

  void AddAppListLauncherItem() {
    ash::ShelfItem app_list;
    app_list.type = ash::TYPE_APP_LIST;
    model_->Add(app_list);
  }

  void InitLauncherController() {
    AddAppListLauncherItem();
    launcher_controller_.reset(
        new ChromeLauncherController(profile(), model_.get()));
    if (!ash::Shell::HasInstance())
      SetShelfItemDelegateManager(item_delegate_manager_);
    launcher_controller_->Init();
  }

  void InitLauncherControllerWithBrowser() {
    chrome::NewTab(browser());
    BrowserList::SetLastActive(browser());
    InitLauncherController();
  }

  void SetAppIconLoader(extensions::AppIconLoader* loader) {
    launcher_controller_->SetAppIconLoaderForTest(loader);
  }

  void SetAppTabHelper(ChromeLauncherController::AppTabHelper* helper) {
    launcher_controller_->SetAppTabHelperForTest(helper);
  }

  void SetShelfItemDelegateManager(ash::ShelfItemDelegateManager* manager) {
    launcher_controller_->SetShelfItemDelegateManagerForTest(manager);
  }

  void InsertPrefValue(base::ListValue* pref_value,
                       int index,
                       const std::string& extension_id) {
    base::DictionaryValue* entry = new base::DictionaryValue();
    entry->SetString(ash::kPinnedAppsPrefAppIDPath, extension_id);
    pref_value->Insert(index, entry);
  }

  // Gets the currently configured app launchers from the controller.
  void GetAppLaunchers(ChromeLauncherController* controller,
                       std::vector<std::string>* launchers) {
    launchers->clear();
    for (ash::ShelfItems::const_iterator iter(model_->items().begin());
         iter != model_->items().end(); ++iter) {
      ChromeLauncherController::IDToItemControllerMap::const_iterator
          entry(controller->id_to_item_controller_map_.find(iter->id));
      if (iter->type == ash::TYPE_APP_SHORTCUT &&
          entry != controller->id_to_item_controller_map_.end()) {
        launchers->push_back(entry->second->app_id());
      }
    }
  }

  // Get the setup of the currently shown launcher items in one string.
  // Each pinned element will start with a big letter, each running but not
  // pinned V1 app will start with a small letter and each running but not
  // pinned V2 app will start with a '*' + small letter.
  std::string GetPinnedAppStatus() {
    std::string result;
    for (int i = 0; i < model_->item_count(); i++) {
      if (!result.empty())
        result.append(", ");
      switch (model_->items()[i].type) {
        case ash::TYPE_PLATFORM_APP:
            result+= "*";
            // FALLTHROUGH
        case ash::TYPE_WINDOWED_APP: {
          const std::string& app =
              launcher_controller_->GetAppIDForShelfID(model_->items()[i].id);
            if (app == extension1_->id()) {
              result += "app1";
              EXPECT_FALSE(
                  launcher_controller_->IsAppPinned(extension1_->id()));
            } else if (app == extension2_->id()) {
              result += "app2";
              EXPECT_FALSE(
                  launcher_controller_->IsAppPinned(extension2_->id()));
            } else if (app == extension3_->id()) {
              result += "app3";
              EXPECT_FALSE(
                  launcher_controller_->IsAppPinned(extension3_->id()));
            } else if (app == extension4_->id()) {
              result += "app4";
              EXPECT_FALSE(
                  launcher_controller_->IsAppPinned(extension4_->id()));
            } else if (app == extension5_->id()) {
              result += "app5";
              EXPECT_FALSE(
                  launcher_controller_->IsAppPinned(extension5_->id()));
            } else if (app == extension6_->id()) {
              result += "app6";
              EXPECT_FALSE(
                  launcher_controller_->IsAppPinned(extension6_->id()));
            } else if (app == extension7_->id()) {
              result += "app7";
              EXPECT_FALSE(
                  launcher_controller_->IsAppPinned(extension7_->id()));
            } else if (app == extension8_->id()) {
              result += "app8";
              EXPECT_FALSE(
                  launcher_controller_->IsAppPinned(extension8_->id()));
            } else {
              result += "unknown";
            }
            break;
          }
        case ash::TYPE_APP_SHORTCUT: {
          const std::string& app =
              launcher_controller_->GetAppIDForShelfID(model_->items()[i].id);
            if (app == extension1_->id()) {
              result += "App1";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
            } else if (app == extension2_->id()) {
              result += "App2";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
            } else if (app == extension3_->id()) {
              result += "App3";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
            } else if (app == extension4_->id()) {
              result += "App4";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));
            } else if (app == extension5_->id()) {
              result += "App5";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension5_->id()));
            } else if (app == extension6_->id()) {
              result += "App6";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension6_->id()));
            } else if (app == extension7_->id()) {
              result += "App7";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension7_->id()));
            } else if (app == extension8_->id()) {
              result += "App8";
              EXPECT_TRUE(launcher_controller_->IsAppPinned(extension8_->id()));
            } else {
              result += "unknown";
            }
            break;
          }
        case ash::TYPE_BROWSER_SHORTCUT:
          result += "Chrome";
          break;
        case ash::TYPE_APP_LIST:
          result += "AppList";
          break;
        default:
          result += "Unknown";
          break;
      }
    }
    return result;
  }

  // Set the index at which the chrome icon should be.
  void SetShelfChromeIconIndex(int index) {
    profile()->GetTestingPrefService()->SetInteger(prefs::kShelfChromeIconIndex,
                                                   index);
  }

  // Remember the order of unpinned but running applications for the current
  // user.
  void RememberUnpinnedRunningApplicationOrder() {
    launcher_controller_->RememberUnpinnedRunningApplicationOrder();
  }

  // Restore the order of running but unpinned applications for a given user.
  void RestoreUnpinnedRunningApplicationOrder(const std::string& user_id) {
    launcher_controller_->RestoreUnpinnedRunningApplicationOrder(user_id);
  }

  // Needed for extension service & friends to work.
  scoped_refptr<Extension> extension1_;
  scoped_refptr<Extension> extension2_;
  scoped_refptr<Extension> extension3_;
  scoped_refptr<Extension> extension4_;
  scoped_refptr<Extension> extension5_;
  scoped_refptr<Extension> extension6_;
  scoped_refptr<Extension> extension7_;
  scoped_refptr<Extension> extension8_;
  scoped_ptr<ChromeLauncherController> launcher_controller_;
  scoped_ptr<TestShelfModelObserver> model_observer_;
  scoped_ptr<ash::ShelfModel> model_;

  // |item_delegate_manager_| owns |test_controller_|.
  LauncherItemController* test_controller_;

  ExtensionService* extension_service_;

  ash::ShelfItemDelegateManager* item_delegate_manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherControllerTest);
};

#if defined(OS_CHROMEOS)
// A browser window proxy which is able to associate an aura native window with
// it.
class TestBrowserWindowAura : public TestBrowserWindow {
 public:
  // |native_window| will still be owned by the caller after the constructor
  // was called.
  explicit TestBrowserWindowAura(aura::Window* native_window)
      : native_window_(native_window) {
  }
  virtual ~TestBrowserWindowAura() {}

  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE {
    return native_window_.get();
  }

  Browser* browser() { return browser_.get(); }

  void CreateBrowser(const Browser::CreateParams& params) {
    Browser::CreateParams create_params = params;
    create_params.window = this;
    browser_.reset(new Browser(create_params));
  }

 private:
  scoped_ptr<Browser> browser_;
  scoped_ptr<aura::Window> native_window_;

  DISALLOW_COPY_AND_ASSIGN(TestBrowserWindowAura);
};

// Creates a test browser window which has a native window.
scoped_ptr<TestBrowserWindowAura> CreateTestBrowserWindow(
    const Browser::CreateParams& params) {
  // Create a window.
  aura::Window* window = new aura::Window(NULL);
  window->set_id(0);
  window->SetType(ui::wm::WINDOW_TYPE_NORMAL);
  window->Init(aura::WINDOW_LAYER_TEXTURED);
  window->Show();

  scoped_ptr<TestBrowserWindowAura> browser_window(
      new TestBrowserWindowAura(window));
  browser_window->CreateBrowser(params);
  return browser_window.Pass();
}

// Watches WebContents and blocks until it is destroyed. This is needed for
// the destruction of a V2 application.
class WebContentsDestroyedWatcher : public content::WebContentsObserver {
 public:
  explicit WebContentsDestroyedWatcher(content::WebContents* web_contents)
      : content::WebContentsObserver(web_contents),
        message_loop_runner_(new content::MessageLoopRunner) {
    EXPECT_TRUE(web_contents != NULL);
  }
  virtual ~WebContentsDestroyedWatcher() {}

  // Waits until the WebContents is destroyed.
  void Wait() {
    message_loop_runner_->Run();
  }

 private:
  // Overridden WebContentsObserver methods.
  virtual void WebContentsDestroyed() OVERRIDE {
    message_loop_runner_->Quit();
  }

  scoped_refptr<content::MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestroyedWatcher);
};

// A V1 windowed application.
class V1App : public TestBrowserWindow {
 public:
  V1App(Profile* profile, const std::string& app_name) {
    // Create a window.
    native_window_.reset(new aura::Window(NULL));
    native_window_->set_id(0);
    native_window_->SetType(ui::wm::WINDOW_TYPE_POPUP);
    native_window_->Init(aura::WINDOW_LAYER_TEXTURED);
    native_window_->Show();
    aura::client::ParentWindowWithContext(native_window_.get(),
                                          ash::Shell::GetPrimaryRootWindow(),
                                          gfx::Rect(10, 10, 20, 30));
    Browser::CreateParams params =
        Browser::CreateParams::CreateForApp(kCrxAppPrefix + app_name,
                                            true /* trusted_source */,
                                            gfx::Rect(),
                                            profile,
                                            chrome::HOST_DESKTOP_TYPE_ASH);
    params.window = this;
    browser_.reset(new Browser(params));
    chrome::AddTabAt(browser_.get(), GURL(), 0, true);
  }

  virtual ~V1App() {
    // close all tabs. Note that we do not need to destroy the browser itself.
    browser_->tab_strip_model()->CloseAllTabs();
  }

  Browser* browser() { return browser_.get(); }

  // TestBrowserWindow override:
  virtual gfx::NativeWindow GetNativeWindow() OVERRIDE {
    return native_window_.get();
  }

 private:
  // The associated browser with this app.
  scoped_ptr<Browser> browser_;

  // The native window we use.
  scoped_ptr<aura::Window> native_window_;

  DISALLOW_COPY_AND_ASSIGN(V1App);
};

// A V2 application which gets created with an |extension| and for a |profile|.
// Upon destruction it will properly close the application.
class V2App {
 public:
  V2App(Profile* profile, const extensions::Extension* extension) {
    window_ = new apps::AppWindow(profile, new ChromeAppDelegate(), extension);
    apps::AppWindow::CreateParams params = apps::AppWindow::CreateParams();
    window_->Init(
        GURL(std::string()), new apps::AppWindowContentsImpl(window_), params);
  }

  virtual ~V2App() {
    WebContentsDestroyedWatcher destroyed_watcher(window_->web_contents());
    window_->GetBaseWindow()->Close();
    destroyed_watcher.Wait();
  }

  apps::AppWindow* window() { return window_; }

 private:
  // The app window which represents the application. Note that the window
  // deletes itself asynchronously after window_->GetBaseWindow()->Close() gets
  // called.
  apps::AppWindow* window_;

  DISALLOW_COPY_AND_ASSIGN(V2App);
};

// The testing framework to test multi profile scenarios.
class MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest
    : public ChromeLauncherControllerTest {
 protected:
  MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest() {
  }

  virtual ~MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest() {
  }

  // Overwrite the Setup function to enable multi profile and needed objects.
  virtual void SetUp() OVERRIDE {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));

    ASSERT_TRUE(profile_manager_->SetUp());

    // AvatarMenu and multiple profiles works after user logged in.
    profile_manager_->SetLoggedIn(true);

    // Initialize the UserManager singleton to a fresh FakeUserManager instance.
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(new chromeos::FakeUserManager));

    // Initialize the rest.
    ChromeLauncherControllerTest::SetUp();

    // Get some base objects.
    session_delegate()->set_logged_in_users(2);
    shell_delegate_ = static_cast<ash::test::TestShellDelegate*>(
        ash::Shell::GetInstance()->delegate());
    shell_delegate_->set_multi_profiles_enabled(true);
  }

  virtual void TearDown() {
    ChromeLauncherControllerTest::TearDown();
    user_manager_enabler_.reset();
    for (ProfileToNameMap::iterator it = created_profiles_.begin();
         it != created_profiles_.end(); ++it)
      profile_manager_->DeleteTestingProfile(it->second);

    // A Task is leaked if we don't destroy everything, then run the message
    // loop.
    base::MessageLoop::current()->PostTask(FROM_HERE,
                                           base::MessageLoop::QuitClosure());
    base::MessageLoop::current()->Run();
  }

  // Creates a profile for a given |user_name|. Note that this class will keep
  // the ownership of the created object.
  TestingProfile* CreateMultiUserProfile(const std::string& user_name) {
    std::string email_string = user_name + "@example.com";
    static_cast<ash::test::TestSessionStateDelegate*>(
        ash::Shell::GetInstance()->session_state_delegate())
        ->AddUser(email_string);
    // Add a user to the fake user manager.
    session_delegate()->AddUser(email_string);
    GetFakeUserManager()->AddUser(email_string);

    GetFakeUserManager()->UserLoggedIn(
        email_string,
        email_string + kUserIdHashSuffix,
        false);

    std::string profile_name =
        chrome::kProfileDirPrefix + email_string + kUserIdHashSuffix;
    TestingProfile* profile = profile_manager()->CreateTestingProfile(
        profile_name,
        scoped_ptr<PrefServiceSyncable>(),
        ASCIIToUTF16(email_string), 0, std::string(),
        TestingProfile::TestingFactories());
    profile->set_profile_name(email_string);
    EXPECT_TRUE(profile);
    // Remember the profile name so that we can destroy it upon destruction.
    created_profiles_[profile] = profile_name;
    if (chrome::MultiUserWindowManager::GetInstance())
      chrome::MultiUserWindowManager::GetInstance()->AddUser(profile);
    if (launcher_controller_)
      launcher_controller_->AdditionalUserAddedToSession(profile);
    return profile;
  }

  // Switch to another user.
  void SwitchActiveUser(const std::string& name) {
    session_delegate()->SwitchActiveUser(name);
    GetFakeUserManager()->SwitchActiveUser(name);
    chrome::MultiUserWindowManagerChromeOS* manager =
        static_cast<chrome::MultiUserWindowManagerChromeOS*>(
            chrome::MultiUserWindowManager::GetInstance());
    manager->SetAnimationSpeedForTest(
        chrome::MultiUserWindowManagerChromeOS::ANIMATION_SPEED_DISABLED);
    manager->ActiveUserChanged(name);
    launcher_controller_->browser_status_monitor_for_test()->
        ActiveUserChanged(name);
    launcher_controller_->app_window_controller_for_test()->
        ActiveUserChanged(name);
  }

  // Creates a browser with a |profile| and load a tab with a |title| and |url|.
  Browser* CreateBrowserAndTabWithProfile(Profile* profile,
                                          const std::string& title,
                                          const std::string& url) {
    Browser::CreateParams params(profile, chrome::HOST_DESKTOP_TYPE_ASH);
    Browser* browser = chrome::CreateBrowserWithTestWindowForParams(&params);
    chrome::NewTab(browser);

    BrowserList::SetLastActive(browser);
    NavigateAndCommitActiveTabWithTitle(
        browser, GURL(url), ASCIIToUTF16(title));
    return browser;
  }

  // Creates a running V1 application.
  // Note that with the use of the app_tab_helper as done below, this is only
  // usable with a single v1 application.
  V1App* CreateRunningV1App(Profile* profile,
                            const std::string& app_name,
                            const std::string& url) {
    V1App* v1_app = new V1App(profile, app_name);
    // Create a new app tab helper and assign it to the launcher so that this
    // app gets properly detected.
    // TODO(skuhne): Create a more intelligent app tab helper which is able to
    // detect all running apps properly.
    TestAppTabHelperImpl* app_tab_helper = new TestAppTabHelperImpl;
    app_tab_helper->SetAppID(
        v1_app->browser()->tab_strip_model()->GetWebContentsAt(0),
        app_name);
    SetAppTabHelper(app_tab_helper);

    NavigateAndCommitActiveTabWithTitle(
        v1_app->browser(), GURL(url), ASCIIToUTF16(""));
    return v1_app;
  }

  ash::test::TestSessionStateDelegate* session_delegate() {
    return static_cast<ash::test::TestSessionStateDelegate*>(
        ash::Shell::GetInstance()->session_state_delegate());
  }
  ash::test::TestShellDelegate* shell_delegate() { return shell_delegate_; }

  // Override BrowserWithTestWindowTest:
  virtual TestingProfile* CreateProfile() OVERRIDE {
    return CreateMultiUserProfile("user1");
  }
  virtual void DestroyProfile(TestingProfile* profile) OVERRIDE {
    // Delete the profile through our profile manager.
    ProfileToNameMap::iterator it = created_profiles_.find(profile);
    DCHECK(it != created_profiles_.end());
    profile_manager_->DeleteTestingProfile(it->second);
    created_profiles_.erase(it);
  }

 private:
  typedef std::map<Profile*, std::string> ProfileToNameMap;
  TestingProfileManager* profile_manager() { return profile_manager_.get(); }

  chromeos::FakeUserManager* GetFakeUserManager() {
    return static_cast<chromeos::FakeUserManager*>(
        user_manager::UserManager::Get());
  }

  scoped_ptr<TestingProfileManager> profile_manager_;
  scoped_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;

  ash::test::TestShellDelegate* shell_delegate_;

  ProfileToNameMap created_profiles_;

  DISALLOW_COPY_AND_ASSIGN(
      MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest);
};
#endif  // defined(OS_CHROMEOS)


TEST_F(ChromeLauncherControllerTest, DefaultApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App3", GetPinnedAppStatus());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced reverse
// order.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsReverseOrder) {
  InitLauncherController();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  SetShelfChromeIconIndex(0);
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the shelf - behind the
  // chrome icon.
  ash::ShelfItem item;
  extension_service_->AddExtension(extension3_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_EQ("AppList, Chrome, App3", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_EQ("AppList, Chrome, App2, App3", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned them (on another system) when they are synced random
// order.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsRandomOrder) {
  InitLauncherController();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  SetShelfChromeIconIndex(0);
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the launcher - behind the
  // chrome icon.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());
}

// Check that the restauration of launcher items is happening in the same order
// as the user has pinned / moved them (on another system) when they are synced
// random order - including the chrome icon.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsRandomOrderChromeMoved) {
  InitLauncherController();

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  SetShelfChromeIconIndex(1);
  // Model should only contain the browser shortcut and app list items.
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());

  // Installing |extension2_| should add it to the shelf - behind the
  // chrome icon.
  ash::ShelfItem item;
  extension_service_->AddExtension(extension2_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, Chrome, App2", GetPinnedAppStatus());

  // Installing |extension1_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension1_.get());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_EQ("AppList, App1, Chrome, App2", GetPinnedAppStatus());

  // Installing |extension3_| should add it to the launcher - behind the
  // chrome icon, but in first location.
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, App1, Chrome, App2, App3", GetPinnedAppStatus());
}

// Check that syncing to a different state does the correct thing.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAppsResyncOrder) {
  InitLauncherController();
  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  InsertPrefValue(&policy_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value.DeepCopy());
  // The shelf layout has always one static item at the beginning (App List).
  SetShelfChromeIconIndex(0);
  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ("AppList, Chrome, App2", GetPinnedAppStatus());
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2", GetPinnedAppStatus());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App1, App2, App3", GetPinnedAppStatus());

  // Change the order with increasing chrome position and decreasing position.
  base::ListValue policy_value1;
  InsertPrefValue(&policy_value1, 0, extension3_->id());
  InsertPrefValue(&policy_value1, 1, extension1_->id());
  InsertPrefValue(&policy_value1, 2, extension2_->id());
  SetShelfChromeIconIndex(3);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value1.DeepCopy());
  EXPECT_EQ("AppList, App3, App1, App2, Chrome", GetPinnedAppStatus());
  base::ListValue policy_value2;
  InsertPrefValue(&policy_value2, 0, extension2_->id());
  InsertPrefValue(&policy_value2, 1, extension3_->id());
  InsertPrefValue(&policy_value2, 2, extension1_->id());
  SetShelfChromeIconIndex(2);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value2.DeepCopy());
  EXPECT_EQ("AppList, App2, App3, Chrome, App1", GetPinnedAppStatus());

  // Check that the chrome icon can also be at the first possible location.
  SetShelfChromeIconIndex(0);
  base::ListValue policy_value3;
  InsertPrefValue(&policy_value3, 0, extension3_->id());
  InsertPrefValue(&policy_value3, 1, extension2_->id());
  InsertPrefValue(&policy_value3, 2, extension1_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value3.DeepCopy());
  EXPECT_EQ("AppList, Chrome, App3, App2, App1", GetPinnedAppStatus());

  // Check that unloading of extensions works as expected.
  extension_service_->UnloadExtension(extension1_->id(),
                                      UnloadedExtensionInfo::REASON_UNINSTALL);
  EXPECT_EQ("AppList, Chrome, App3, App2", GetPinnedAppStatus());

  extension_service_->UnloadExtension(extension2_->id(),
                                      UnloadedExtensionInfo::REASON_UNINSTALL);
  EXPECT_EQ("AppList, Chrome, App3", GetPinnedAppStatus());

  // Check that an update of an extension does not crash the system.
  extension_service_->UnloadExtension(extension3_->id(),
                                      UnloadedExtensionInfo::REASON_UPDATE);
  EXPECT_EQ("AppList, Chrome, App3", GetPinnedAppStatus());
}

// Check that simple locking of an application will 'create' a launcher item.
TEST_F(ChromeLauncherControllerTest, CheckLockApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension2_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[2].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension2_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension2_->id()));
}

// Check that multiple locks of an application will be properly handled.
TEST_F(ChromeLauncherControllerTest, CheckMultiLockApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  for (int i = 0; i < 2; i++) {
    launcher_controller_->LockV1AppWithID(extension1_->id());

    EXPECT_EQ(3, model_->item_count());
    EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[2].type);
    EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
    EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(
        extension1_->id()));
  }

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[2].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));
}

// Check that already pinned items are not effected by locks.
TEST_F(ChromeLauncherControllerTest, CheckAlreadyPinnedLockApps) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  launcher_controller_->PinAppWithID(extension1_->id());
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnpinAppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
}

// Check that already pinned items which get locked stay after unpinning.
TEST_F(ChromeLauncherControllerTest, CheckPinnedAppsStayAfterUnlock) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnpinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[2].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
}

#if defined(OS_CHROMEOS)
// Check that running applications wich are not pinned get properly restored
// upon user change.
TEST_F(ChromeLauncherControllerTest, CheckRunningAppOrder) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());

  // Add a few running applications.
  launcher_controller_->LockV1AppWithID(extension1_->id());
  launcher_controller_->LockV1AppWithID(extension2_->id());
  launcher_controller_->LockV1AppWithID(extension3_->id());
  EXPECT_EQ(5, model_->item_count());
  // Note that this not only checks the order of applications but also the
  // running type.
  EXPECT_EQ("AppList, Chrome, app1, app2, app3", GetPinnedAppStatus());

  // Remember the current order of applications for the current user.
  const std::string& current_user_id =
      multi_user_util::GetUserIDFromProfile(profile());
  RememberUnpinnedRunningApplicationOrder();

  // Switch some items and check that restoring a user which was not yet
  // remembered changes nothing.
  model_->Move(2, 3);
  EXPECT_EQ("AppList, Chrome, app2, app1, app3", GetPinnedAppStatus());
  RestoreUnpinnedRunningApplicationOrder("second-fake-user@fake.com");
  EXPECT_EQ("AppList, Chrome, app2, app1, app3", GetPinnedAppStatus());

  // Restoring the stored user should however do the right thing.
  RestoreUnpinnedRunningApplicationOrder(current_user_id);
  EXPECT_EQ("AppList, Chrome, app1, app2, app3", GetPinnedAppStatus());

  // Switch again some items and even delete one - making sure that the missing
  // item gets properly handled.
  model_->Move(3, 4);
  launcher_controller_->UnlockV1AppWithID(extension1_->id());
  EXPECT_EQ("AppList, Chrome, app3, app2", GetPinnedAppStatus());
  RestoreUnpinnedRunningApplicationOrder(current_user_id);
  EXPECT_EQ("AppList, Chrome, app2, app3", GetPinnedAppStatus());

  // Check that removing more items does not crash and changes nothing.
  launcher_controller_->UnlockV1AppWithID(extension2_->id());
  RestoreUnpinnedRunningApplicationOrder(current_user_id);
  EXPECT_EQ("AppList, Chrome, app3", GetPinnedAppStatus());
  launcher_controller_->UnlockV1AppWithID(extension3_->id());
  RestoreUnpinnedRunningApplicationOrder(current_user_id);
  EXPECT_EQ("AppList, Chrome", GetPinnedAppStatus());
}

// Check that with multi profile V1 apps are properly added / removed from the
// shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1AppUpdateOnUserSwitch) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  EXPECT_EQ(2, model_->item_count());
  {
    // Create a "windowed gmail app".
    scoped_ptr<V1App> v1_app(CreateRunningV1App(
        profile(), extension_misc::kGmailAppId, gmail_url));
    EXPECT_EQ(3, model_->item_count());

    // After switching to a second user the item should be gone.
    std::string user2 = "user2";
    TestingProfile* profile2 = CreateMultiUserProfile(user2);
    SwitchActiveUser(profile2->GetProfileName());
    EXPECT_EQ(2, model_->item_count());

    // After switching back the item should be back.
    SwitchActiveUser(profile()->GetProfileName());
    EXPECT_EQ(3, model_->item_count());
    // Note we destroy now the gmail app with the closure end.
  }
  EXPECT_EQ(2, model_->item_count());
}

// Check edge cases with multi profile V1 apps in the shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1AppUpdateOnUserSwitchEdgecases) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  // First test: Create an app when the user is not active.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  {
    // Create a "windowed gmail app".
    scoped_ptr<V1App> v1_app(CreateRunningV1App(
        profile2, extension_misc::kGmailAppId, gmail_url));
    EXPECT_EQ(2, model_->item_count());

    // However - switching to the user should show it.
    SwitchActiveUser(profile2->GetProfileName());
    EXPECT_EQ(3, model_->item_count());

    // Second test: Remove the app when the user is not active and see that it
    // works.
    SwitchActiveUser(profile()->GetProfileName());
    EXPECT_EQ(2, model_->item_count());
    // Note: the closure ends and the browser will go away.
  }
  EXPECT_EQ(2, model_->item_count());
  SwitchActiveUser(profile2->GetProfileName());
  EXPECT_EQ(2, model_->item_count());
  SwitchActiveUser(profile()->GetProfileName());
  EXPECT_EQ(2, model_->item_count());
}

// Check edge case where a visiting V1 app gets closed (crbug.com/321374).
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1CloseOnVisitingDesktop) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  chrome::MultiUserWindowManager* manager =
      chrome::MultiUserWindowManager::GetInstance();

  // First create an app when the user is active.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  {
    // Create a "windowed gmail app".
    scoped_ptr<V1App> v1_app(CreateRunningV1App(
        profile(),
        extension_misc::kGmailAppId,
        kGmailLaunchURL));
    EXPECT_EQ(3, model_->item_count());

    // Transfer the app to the other screen and switch users.
    manager->ShowWindowForUser(v1_app->browser()->window()->GetNativeWindow(),
                               user2);
    EXPECT_EQ(3, model_->item_count());
    SwitchActiveUser(profile2->GetProfileName());
    EXPECT_EQ(2, model_->item_count());
  }
  // After the app was destroyed, switch back. (which caused already a crash).
  SwitchActiveUser(profile()->GetProfileName());

  // Create the same app again - which was also causing the crash.
  EXPECT_EQ(2, model_->item_count());
  {
    // Create a "windowed gmail app".
    scoped_ptr<V1App> v1_app(CreateRunningV1App(
        profile(),
        extension_misc::kGmailAppId,
        kGmailLaunchURL));
    EXPECT_EQ(3, model_->item_count());
  }
  SwitchActiveUser(profile2->GetProfileName());
  EXPECT_EQ(2, model_->item_count());
}

// Check edge cases with multi profile V1 apps in the shelf.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1AppUpdateOnUserSwitchEdgecases2) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  TestAppTabHelperImpl* app_tab_helper = new TestAppTabHelperImpl;
  SetAppTabHelper(app_tab_helper);

  // First test: Create an app when the user is not active.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  SwitchActiveUser(profile2->GetProfileName());
  {
    // Create a "windowed gmail app".
    scoped_ptr<V1App> v1_app(CreateRunningV1App(
        profile(), extension_misc::kGmailAppId, gmail_url));
    EXPECT_EQ(2, model_->item_count());

    // However - switching to the user should show it.
    SwitchActiveUser(profile()->GetProfileName());
    EXPECT_EQ(3, model_->item_count());

    // Second test: Remove the app when the user is not active and see that it
    // works.
    SwitchActiveUser(profile2->GetProfileName());
    EXPECT_EQ(2, model_->item_count());
    v1_app.reset();
  }
  EXPECT_EQ(2, model_->item_count());
  SwitchActiveUser(profile()->GetProfileName());
  EXPECT_EQ(2, model_->item_count());
  SwitchActiveUser(profile2->GetProfileName());
  EXPECT_EQ(2, model_->item_count());
}

// Check that activating an item which is on another user's desktop, will bring
// it back.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       TestLauncherActivationPullsBackWindow) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  chrome::MultiUserWindowManager* manager =
      chrome::MultiUserWindowManager::GetInstance();

  // Add two users to the window manager.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  manager->AddUser(profile());
  manager->AddUser(profile2);
  const std::string& current_user =
      multi_user_util::GetUserIDFromProfile(profile());

  // Create a browser window with a native window for the current user.
  scoped_ptr<BrowserWindow> browser_window(CreateTestBrowserWindow(
      Browser::CreateParams(profile(), chrome::HOST_DESKTOP_TYPE_ASH)));
  aura::Window* window = browser_window->GetNativeWindow();
  manager->SetWindowOwner(window, current_user);

  // Check that an activation of the window on its owner's desktop does not
  // change the visibility to another user.
  launcher_controller_->ActivateWindowOrMinimizeIfActive(browser_window.get(),
                                                         false);
  EXPECT_TRUE(manager->IsWindowOnDesktopOfUser(window, current_user));

  // Transfer the window to another user's desktop and check that activating it
  // does pull it back to that user.
  manager->ShowWindowForUser(window, user2);
  EXPECT_FALSE(manager->IsWindowOnDesktopOfUser(window, current_user));
  launcher_controller_->ActivateWindowOrMinimizeIfActive(browser_window.get(),
                                                         false);
  EXPECT_TRUE(manager->IsWindowOnDesktopOfUser(window, current_user));
}
#endif

// Check that lock -> pin -> unlock -> unpin does properly transition.
TEST_F(ChromeLauncherControllerTest, CheckLockPinUnlockUnpin) {
  InitLauncherController();
  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->LockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_WINDOWED_APP, model_->items()[2].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->PinAppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnlockV1AppWithID(extension1_->id());

  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(
      launcher_controller_->IsWindowedAppInLauncher(extension1_->id()));

  launcher_controller_->UnpinAppWithID(extension1_->id());

  EXPECT_EQ(2, model_->item_count());
}

// Check that a locked (windowed V1 application) will be properly converted
// between locked and pinned when the order gets changed through a profile /
// policy change.
TEST_F(ChromeLauncherControllerTest, RestoreDefaultAndLockedAppsResyncOrder) {
  InitLauncherController();
  base::ListValue policy_value0;
  InsertPrefValue(&policy_value0, 0, extension1_->id());
  InsertPrefValue(&policy_value0, 1, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value0.DeepCopy());
  // The shelf layout has always one static item at the beginning (App List).
  SetShelfChromeIconIndex(0);
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("AppList, Chrome, App1", GetPinnedAppStatus());
  extension_service_->AddExtension(extension2_.get());
  // No new app icon will be generated.
  EXPECT_EQ("AppList, Chrome, App1", GetPinnedAppStatus());
  // Add the app as locked app which will add it (un-pinned).
  launcher_controller_->LockV1AppWithID(extension2_->id());
  EXPECT_EQ("AppList, Chrome, App1, app2", GetPinnedAppStatus());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App1, App3, app2", GetPinnedAppStatus());

  // Now request to pin all items which should convert the locked item into a
  // pinned item.
  base::ListValue policy_value1;
  InsertPrefValue(&policy_value1, 0, extension3_->id());
  InsertPrefValue(&policy_value1, 1, extension2_->id());
  InsertPrefValue(&policy_value1, 2, extension1_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value1.DeepCopy());
  EXPECT_EQ("AppList, Chrome, App3, App2, App1", GetPinnedAppStatus());

  // Going back to a status where there is no requirement for app 2 to be pinned
  // should convert it back to locked but not pinned and state. The position
  // is determined by the |ShelfModel|'s weight system and since running
  // applications are not allowed to be mixed with shortcuts, it should show up
  // at the end of the list.
  base::ListValue policy_value2;
  InsertPrefValue(&policy_value2, 0, extension3_->id());
  InsertPrefValue(&policy_value2, 1, extension1_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value2.DeepCopy());
  EXPECT_EQ("AppList, Chrome, App3, App1, app2", GetPinnedAppStatus());

  // Removing an item should simply close it and everything should shift.
  base::ListValue policy_value3;
  InsertPrefValue(&policy_value3, 0, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value3.DeepCopy());
  EXPECT_EQ("AppList, Chrome, App3, app2", GetPinnedAppStatus());
}

// Check that a running and not pinned V2 application will be properly converted
// between locked and pinned when the order gets changed through a profile /
// policy change.
TEST_F(ChromeLauncherControllerTest,
       RestoreDefaultAndRunningV2AppsResyncOrder) {
  InitLauncherController();
  base::ListValue policy_value0;
  InsertPrefValue(&policy_value0, 0, extension1_->id());
  InsertPrefValue(&policy_value0, 1, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value0.DeepCopy());
  // The shelf layout has always one static item at the beginning (app List).
  SetShelfChromeIconIndex(0);
  extension_service_->AddExtension(extension1_.get());
  EXPECT_EQ("AppList, Chrome, App1", GetPinnedAppStatus());
  extension_service_->AddExtension(extension2_.get());
  // No new app icon will be generated.
  EXPECT_EQ("AppList, Chrome, App1", GetPinnedAppStatus());
  // Add the app as an unpinned but running V2 app.
  CreateRunningV2App(extension2_->id());
  EXPECT_EQ("AppList, Chrome, App1, *app2", GetPinnedAppStatus());
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ("AppList, Chrome, App1, App3, *app2", GetPinnedAppStatus());

  // Now request to pin all items which should convert the locked item into a
  // pinned item.
  base::ListValue policy_value1;
  InsertPrefValue(&policy_value1, 0, extension3_->id());
  InsertPrefValue(&policy_value1, 1, extension2_->id());
  InsertPrefValue(&policy_value1, 2, extension1_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value1.DeepCopy());
  EXPECT_EQ("AppList, Chrome, App3, App2, App1", GetPinnedAppStatus());

  // Going back to a status where there is no requirement for app 2 to be pinned
  // should convert it back to running V2 app. Since the position is determined
  // by the |ShelfModel|'s weight system, it will be after last pinned item.
  base::ListValue policy_value2;
  InsertPrefValue(&policy_value2, 0, extension3_->id());
  InsertPrefValue(&policy_value2, 1, extension1_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value2.DeepCopy());
  EXPECT_EQ("AppList, Chrome, App3, App1, *app2", GetPinnedAppStatus());

  // Removing an item should simply close it and everything should shift.
  base::ListValue policy_value3;
  InsertPrefValue(&policy_value3, 0, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  policy_value3.DeepCopy());
  EXPECT_EQ("AppList, Chrome, App3, *app2", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned. Check that when
// switching between the two users, the state gets properly set.
TEST_F(ChromeLauncherControllerTest, UserSwitchIconRestore) {
  base::ListValue user_a;
  base::ListValue user_b;
  SetUpMultiUserScenario(&user_a, &user_b);
  // Show user 1.
  SetShelfChromeIconIndex(6);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_a.DeepCopy());
  EXPECT_EQ("AppList, App1, App2, App3, App4, App5, App6, Chrome",
            GetPinnedAppStatus());

  // Show user 2.
  SetShelfChromeIconIndex(4);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_b.DeepCopy());

  EXPECT_EQ("AppList, App7, App8, Chrome", GetPinnedAppStatus());

  // Switch back to 1.
  SetShelfChromeIconIndex(8);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_a.DeepCopy());
  EXPECT_EQ("AppList, App1, App2, App3, App4, App5, App6, Chrome",
            GetPinnedAppStatus());

  // Switch back to 2.
  SetShelfChromeIconIndex(4);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_b.DeepCopy());
  EXPECT_EQ("AppList, App7, App8, Chrome", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned, and one user has an
// application running. Check that when switching between the two users, the
// state gets properly set.
TEST_F(ChromeLauncherControllerTest, UserSwitchIconRestoreWithRunningV2App) {
  base::ListValue user_a;
  base::ListValue user_b;
  SetUpMultiUserScenario(&user_a, &user_b);

  // Run App1 and assume that it is a V2 app.
  CreateRunningV2App(extension1_->id());

  // Show user 1.
  SetShelfChromeIconIndex(6);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_a.DeepCopy());
  EXPECT_EQ("AppList, App1, App2, App3, App4, App5, App6, Chrome",
            GetPinnedAppStatus());

  // Show user 2.
  SetShelfChromeIconIndex(4);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_b.DeepCopy());

  EXPECT_EQ("AppList, App7, App8, Chrome, *app1", GetPinnedAppStatus());

  // Switch back to 1.
  SetShelfChromeIconIndex(8);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_a.DeepCopy());
  EXPECT_EQ("AppList, App1, App2, App3, App4, App5, App6, Chrome",
            GetPinnedAppStatus());

  // Switch back to 2.
  SetShelfChromeIconIndex(4);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_b.DeepCopy());
  EXPECT_EQ("AppList, App7, App8, Chrome, *app1", GetPinnedAppStatus());
}

// Each user has a different set of applications pinned, and one user has an
// application running. The chrome icon is not the last item in the list.
// Check that when switching between the two users, the state gets properly set.
// There was once a bug associated with this.
TEST_F(ChromeLauncherControllerTest,
       UserSwitchIconRestoreWithRunningV2AppChromeInMiddle) {
  base::ListValue user_a;
  base::ListValue user_b;
  SetUpMultiUserScenario(&user_a, &user_b);

  // Run App1 and assume that it is a V2 app.
  CreateRunningV2App(extension1_->id());

  // Show user 1.
  SetShelfChromeIconIndex(5);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_a.DeepCopy());
  EXPECT_EQ("AppList, App1, App2, App3, App4, App5, Chrome, App6",
            GetPinnedAppStatus());

  // Show user 2.
  SetShelfChromeIconIndex(4);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_b.DeepCopy());

  EXPECT_EQ("AppList, App7, App8, Chrome, *app1", GetPinnedAppStatus());

  // Switch back to 1.
  SetShelfChromeIconIndex(5);
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                  user_a.DeepCopy());
  EXPECT_EQ("AppList, App1, App2, App3, App4, App5, Chrome, App6",
            GetPinnedAppStatus());
}

TEST_F(ChromeLauncherControllerTest, Policy) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());

  base::ListValue policy_value;
  InsertPrefValue(&policy_value, 0, extension1_->id());
  InsertPrefValue(&policy_value, 1, extension2_->id());
  profile()->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                     policy_value.DeepCopy());

  // Only |extension1_| should get pinned. |extension2_| is specified but not
  // installed, and |extension3_| is part of the default set, but that shouldn't
  // take effect when the policy override is in place.
  InitLauncherController();
  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension2_| should add it to the launcher.
  extension_service_->AddExtension(extension2_.get());
  EXPECT_EQ(4, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[3].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Removing |extension1_| from the policy should be reflected in the launcher.
  policy_value.Remove(0, NULL);
  profile()->GetTestingPrefService()->SetManagedPref(prefs::kPinnedLauncherApps,
                                                     policy_value.DeepCopy());
  EXPECT_EQ(3, model_->item_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension1_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension2_->id()));
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
}

TEST_F(ChromeLauncherControllerTest, UnpinWithUninstall) {
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  InitLauncherController();

  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));

  extension_service_->UnloadExtension(extension3_->id(),
                                      UnloadedExtensionInfo::REASON_UNINSTALL);

  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension4_->id()));
}

TEST_F(ChromeLauncherControllerTest, PrefUpdates) {
  extension_service_->AddExtension(extension2_.get());
  extension_service_->AddExtension(extension3_.get());
  extension_service_->AddExtension(extension4_.get());

  InitLauncherController();

  std::vector<std::string> expected_launchers;
  std::vector<std::string> actual_launchers;
  base::ListValue pref_value;
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Unavailable extensions don't create launcher items.
  InsertPrefValue(&pref_value, 0, extension1_->id());
  InsertPrefValue(&pref_value, 1, extension2_->id());
  InsertPrefValue(&pref_value, 2, extension4_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.push_back(extension2_->id());
  expected_launchers.push_back(extension4_->id());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Redundant pref entries show up only once.
  InsertPrefValue(&pref_value, 2, extension3_->id());
  InsertPrefValue(&pref_value, 2, extension3_->id());
  InsertPrefValue(&pref_value, 5, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.insert(expected_launchers.begin() + 1, extension3_->id());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Order changes are reflected correctly.
  pref_value.Clear();
  InsertPrefValue(&pref_value, 0, extension4_->id());
  InsertPrefValue(&pref_value, 1, extension3_->id());
  InsertPrefValue(&pref_value, 2, extension2_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  std::reverse(expected_launchers.begin(), expected_launchers.end());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Clearing works.
  pref_value.Clear();
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());
  expected_launchers.clear();
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);
}

TEST_F(ChromeLauncherControllerTest, PendingInsertionOrder) {
  extension_service_->AddExtension(extension1_.get());
  extension_service_->AddExtension(extension3_.get());

  InitLauncherController();

  base::ListValue pref_value;
  InsertPrefValue(&pref_value, 0, extension1_->id());
  InsertPrefValue(&pref_value, 1, extension2_->id());
  InsertPrefValue(&pref_value, 2, extension3_->id());
  profile()->GetTestingPrefService()->SetUserPref(prefs::kPinnedLauncherApps,
                                                 pref_value.DeepCopy());

  std::vector<std::string> expected_launchers;
  expected_launchers.push_back(extension1_->id());
  expected_launchers.push_back(extension3_->id());
  std::vector<std::string> actual_launchers;

  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);

  // Install |extension2| and verify it shows up between the other two.
  extension_service_->AddExtension(extension2_.get());
  expected_launchers.insert(expected_launchers.begin() + 1, extension2_->id());
  GetAppLaunchers(launcher_controller_.get(), &actual_launchers);
  EXPECT_EQ(expected_launchers, actual_launchers);
}

// Checks the created menus and menu lists for correctness. It uses the given
// |controller| to create the objects for the given |item| and checks the
// found item count against the |expected_items|. The |title| list contains the
// menu titles in the order of their appearance in the menu (not including the
// application name).
bool CheckMenuCreation(ChromeLauncherController* controller,
                       const ash::ShelfItem& item,
                       size_t expected_items,
                       base::string16 title[],
                       bool is_browser) {
  ChromeLauncherAppMenuItems items = controller->GetApplicationList(item, 0);
  // A new behavior has been added: Only show menus if there is at least one
  // item available.
  if (expected_items < 1 && is_browser) {
    EXPECT_EQ(0u, items.size());
    return items.size() == 0;
  }
  // There should be one item in there: The title.
  EXPECT_EQ(expected_items + 1, items.size());
  EXPECT_FALSE(items[0]->IsEnabled());
  for (size_t i = 0; i < expected_items; i++) {
    EXPECT_EQ(title[i], items[1 + i]->title());
    // Check that the first real item has a leading separator.
    if (i == 1)
      EXPECT_TRUE(items[i]->HasLeadingSeparator());
    else
      EXPECT_FALSE(items[i]->HasLeadingSeparator());
  }

  scoped_ptr<ash::ShelfMenuModel> menu(new LauncherApplicationMenuItemModel(
      controller->GetApplicationList(item, 0)));
  // The first element in the menu is a spacing separator. On some systems
  // (e.g. Windows) such things do not exist. As such we check the existence
  // and adjust dynamically.
  int first_item = menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR ? 1 : 0;
  int expected_menu_items = first_item +
                            (expected_items ? (expected_items + 3) : 2);
  EXPECT_EQ(expected_menu_items, menu->GetItemCount());
  EXPECT_FALSE(menu->IsEnabledAt(first_item));
  if (expected_items) {
    EXPECT_EQ(ui::MenuModel::TYPE_SEPARATOR,
              menu->GetTypeAt(first_item + 1));
  }
  return items.size() == expected_items + 1;
}

// Check that browsers get reflected correctly in the launcher menu.
TEST_F(ChromeLauncherControllerTest, BrowserMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  chrome::NewTab(browser());

  InitLauncherController();

  // Check that the browser list is empty at this time.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id =
      launcher_controller_->GetShelfIDForAppID(extension_misc::kChromeAppId);
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 0, NULL, true));

  // Now make the created browser() visible by adding it to the active browser
  // list.
  BrowserList::SetLastActive(browser());
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("http://test1"), title1);
  base::string16 one_menu_item[] = { title1 };

  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item, true));

  // Create one more browser/window and check that one more was added.
  Browser::CreateParams ash_params(profile(), chrome::HOST_DESKTOP_TYPE_ASH);
  scoped_ptr<Browser> browser2(
      chrome::CreateBrowserWithTestWindowForParams(&ash_params));
  chrome::NewTab(browser2.get());
  BrowserList::SetLastActive(browser2.get());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser2.get(), GURL("http://test2"),
                                      title2);

  // Check that the list contains now two entries - make furthermore sure that
  // the active item is the first entry.
  base::string16 two_menu_items[] = {title1, title2};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 2, two_menu_items, true));

  // Apparently we have to close all tabs we have.
  chrome::CloseTab(browser2.get());
}

#if defined(OS_CHROMEOS)
// Check the multi profile case where only user related browsers should show
// up.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       BrowserMenuGenerationTwoUsers) {
  // Create a browser item in the LauncherController.
  InitLauncherController();

  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id =
      launcher_controller_->GetShelfIDForAppID(extension_misc::kChromeAppId);

  // Check that the menu is empty.
  chrome::NewTab(browser());
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 0, NULL, true));

  // Show the created |browser()| by adding it to the active browser list.
  BrowserList::SetLastActive(browser());
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL("http://test1"), title1);
  base::string16 one_menu_item1[] = { title1 };
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item1, true));

  // Create a browser for another user and check that it is not included in the
  // users running browser list.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  scoped_ptr<Browser> browser2(
      CreateBrowserAndTabWithProfile(profile2, user2, "http://test2"));
  base::string16 one_menu_item2[] = { ASCIIToUTF16(user2) };
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item1, true));

  // Switch to the other user and make sure that only that browser window gets
  // shown.
  SwitchActiveUser(profile2->GetProfileName());
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item2, true));

  // Transferred browsers of other users should not show up in the list.
  chrome::MultiUserWindowManager::GetInstance()->ShowWindowForUser(
      browser()->window()->GetNativeWindow(),
      user2);
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, one_menu_item2, true));

  chrome::CloseTab(browser2.get());
}
#endif  // defined(OS_CHROMEOS)

// Check that V1 apps are correctly reflected in the launcher menu using the
// refocus logic.
// Note that the extension matching logic is tested by the extension system
// and does not need a separate test here.
TEST_F(ChromeLauncherControllerTest, V1AppMenuGeneration) {
  EXPECT_EQ(1U, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());

  InitLauncherControllerWithBrowser();

  // Model should only contain the browser shortcut and app list items.
  EXPECT_EQ(2, model_->item_count());
  EXPECT_FALSE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Installing |extension3_| adds it to the launcher.
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));

  // Check the menu content.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id =
      launcher_controller_->GetShelfIDForAppID(extension_misc::kChromeAppId);

  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));

  // Set the gmail URL to a new tab.
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);

  base::string16 one_menu_item[] = { title1 };
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 1, one_menu_item, false));

  // Create one empty tab.
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(
      browser(),
      GURL("https://bla"),
      title2);

  // and another one with another gmail instance.
  chrome::NewTab(browser());
  base::string16 title3 = ASCIIToUTF16("Test3");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title3);
  base::string16 two_menu_items[] = {title1, title3};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 2, two_menu_items, false));

  // Even though the item is in the V1 app list, it should also be in the
  // browser list.
  base::string16 browser_menu_item[] = {title3};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, browser_menu_item, false));

  // Test that closing of (all) the item(s) does work (and all menus get
  // updated properly).
  launcher_controller_->Close(item_gmail.id);

  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));
  base::string16 browser_menu_item2[] = { title2 };
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 1, browser_menu_item2, false));
}

#if defined(OS_CHROMEOS)
// Check the multi profile case where only user related apps should show up.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V1AppMenuGenerationTwoUsers) {
  // Create a browser item in the LauncherController.
  InitLauncherController();
  chrome::NewTab(browser());

  // Installing |extension3_| adds it to the launcher.
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));

  // Check the menu content.
  ash::ShelfItem item_browser;
  item_browser.type = ash::TYPE_BROWSER_SHORTCUT;
  item_browser.id =
      launcher_controller_->GetShelfIDForAppID(extension_misc::kChromeAppId);

  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));

  // Set the gmail URL to a new tab.
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);

  base::string16 one_menu_item[] = { title1 };
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 1, one_menu_item, false));

  // Create a second profile and switch to that user.
  std::string user2 = "user2";
  TestingProfile* profile2 = CreateMultiUserProfile(user2);
  SwitchActiveUser(profile2->GetProfileName());

  // No item should have content yet.
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 0, NULL, true));
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));

  // Transfer the browser of the first user - it should still not show up.
  chrome::MultiUserWindowManager::GetInstance()->ShowWindowForUser(
      browser()->window()->GetNativeWindow(),
      user2);

  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_browser, 0, NULL, true));
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 0, NULL, false));
}

// Check that V2 applications are creating items properly in the launcher when
// instantiated by the current user.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppHandlingTwoUsers) {
  InitLauncherController();
  // Create a profile for our second user (will be destroyed by the framework).
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  // Check that there is a browser and a app launcher.
  EXPECT_EQ(2, model_->item_count());

  // Add a v2 app.
  V2App v2_app(profile(), extension1_);
  EXPECT_EQ(3, model_->item_count());

  // After switching users the item should go away.
  SwitchActiveUser(profile2->GetProfileName());
  EXPECT_EQ(2, model_->item_count());

  // And it should come back when switching back.
  SwitchActiveUser(profile()->GetProfileName());
  EXPECT_EQ(3, model_->item_count());
}

// Check that V2 applications are creating items properly in edge cases:
// a background user creates a V2 app, gets active and inactive again and then
// deletes the app.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppHandlingTwoUsersEdgeCases) {
  InitLauncherController();
  // Create a profile for our second user (will be destroyed by the framework).
  TestingProfile* profile2 = CreateMultiUserProfile("user2");
  // Check that there is a browser and a app launcher.
  EXPECT_EQ(2, model_->item_count());

  // Switch to an inactive user.
  SwitchActiveUser(profile2->GetProfileName());
  EXPECT_EQ(2, model_->item_count());

  // Add the v2 app to the inactive user and check that no item was added to
  // the launcher.
  {
    V2App v2_app(profile(), extension1_);
    EXPECT_EQ(2, model_->item_count());

    // Switch to the primary user and check that the item is shown.
    SwitchActiveUser(profile()->GetProfileName());
    EXPECT_EQ(3, model_->item_count());

    // Switch to the second user and check that the item goes away - even if the
    // item gets closed.
    SwitchActiveUser(profile2->GetProfileName());
    EXPECT_EQ(2, model_->item_count());
  }

  // After the application was killed there should be still 2 items.
  EXPECT_EQ(2, model_->item_count());

  // Switching then back to the default user should not show the additional item
  // anymore.
  SwitchActiveUser(profile()->GetProfileName());
  EXPECT_EQ(2, model_->item_count());
}

// Check that V2 applications will be made visible on the target desktop if
// another window of the same type got previously teleported there.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppFollowsTeleportedWindow) {
  InitLauncherController();
  chrome::MultiUserWindowManager* manager =
      chrome::MultiUserWindowManager::GetInstance();

  // Create and add three users / profiles, and go to #1's desktop.
  TestingProfile* profile1 = CreateMultiUserProfile("user-1");
  TestingProfile* profile2 = CreateMultiUserProfile("user-2");
  TestingProfile* profile3 = CreateMultiUserProfile("user-3");
  SwitchActiveUser(profile1->GetProfileName());

  // A v2 app for user #1 should be shown first and get hidden when switching to
  // desktop #2.
  V2App v2_app_1(profile1, extension1_);
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  SwitchActiveUser(profile2->GetProfileName());
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());

  // Add a v2 app for user #1 while on desktop #2 should not be shown.
  V2App v2_app_2(profile1, extension1_);
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());

  // Teleport the app from user #1 to the desktop #2 should show it.
  manager->ShowWindowForUser(v2_app_1.window()->GetNativeWindow(),
                             profile2->GetProfileName());
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());

  // Creating a new application for user #1 on desktop #2 should teleport it
  // there automatically.
  V2App v2_app_3(profile1, extension1_);
  EXPECT_TRUE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_3.window()->GetNativeWindow()->IsVisible());

  // Switching back to desktop#1 and creating an app for user #1 should move
  // the app on desktop #1.
  SwitchActiveUser(profile1->GetProfileName());
  V2App v2_app_4(profile1, extension1_);
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_3.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_4.window()->GetNativeWindow()->IsVisible());

  // Switching to desktop #3 and create an app for user #1 there should land on
  // his own desktop (#1).
  SwitchActiveUser(profile3->GetProfileName());
  V2App v2_app_5(profile1, extension1_);
  EXPECT_FALSE(v2_app_5.window()->GetNativeWindow()->IsVisible());
  SwitchActiveUser(profile1->GetProfileName());
  EXPECT_TRUE(v2_app_5.window()->GetNativeWindow()->IsVisible());

  // Switching to desktop #2, hiding the app window and creating an app should
  // teleport there automatically.
  SwitchActiveUser(profile2->GetProfileName());
  v2_app_1.window()->Hide();
  V2App v2_app_6(profile1, extension1_);
  EXPECT_FALSE(v2_app_1.window()->GetNativeWindow()->IsVisible());
  EXPECT_FALSE(v2_app_2.window()->GetNativeWindow()->IsVisible());
  EXPECT_TRUE(v2_app_6.window()->GetNativeWindow()->IsVisible());
}

// Check that V2 applications hide correctly on the shelf when the app window
// is hidden.
TEST_F(MultiProfileMultiBrowserShelfLayoutChromeLauncherControllerTest,
       V2AppHiddenWindows) {
  InitLauncherController();

  TestingProfile* profile2 = CreateMultiUserProfile("user-2");
  SwitchActiveUser(profile()->GetProfileName());
  EXPECT_EQ(2, model_->item_count());

  V2App v2_app_1(profile(), extension1_);
  EXPECT_EQ(3, model_->item_count());
  {
    // Hide and show the app.
    v2_app_1.window()->Hide();
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Show(apps::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(3, model_->item_count());
  }
  {
    // Switch user, hide and show the app and switch back.
    SwitchActiveUser(profile2->GetProfileName());
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Hide();
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Show(apps::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(2, model_->item_count());

    SwitchActiveUser(profile()->GetProfileName());
    EXPECT_EQ(3, model_->item_count());
  }
  {
    // Switch user, hide the app, switch back and then show it again.
    SwitchActiveUser(profile2->GetProfileName());
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Hide();
    EXPECT_EQ(2, model_->item_count());

    SwitchActiveUser(profile()->GetProfileName());
    EXPECT_EQ(2, model_->item_count());

    v2_app_1.window()->Show(apps::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(3, model_->item_count());
  }
  {
    // Create a second app, hide and show it and then hide both apps.
    V2App v2_app_2(profile(), extension1_);
    EXPECT_EQ(3, model_->item_count());

    v2_app_2.window()->Hide();
    EXPECT_EQ(3, model_->item_count());

    v2_app_2.window()->Show(apps::AppWindow::SHOW_ACTIVE);
    EXPECT_EQ(3, model_->item_count());

    v2_app_1.window()->Hide();
    v2_app_2.window()->Hide();
    EXPECT_EQ(2, model_->item_count());
  }
}
#endif  // defined(OS_CHROMEOS)

// Checks that the generated menu list properly activates items.
TEST_F(ChromeLauncherControllerTest, V1AppMenuExecution) {
  InitLauncherControllerWithBrowser();

  // Add |extension3_| to the launcher and add two items.
  GURL gmail = GURL("https://mail.google.com/mail/u");
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title2);

  // Check that the menu is properly set.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  base::string16 two_menu_items[] = {title1, title2};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 2, two_menu_items, false));
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());
  // Execute the second item in the list (which shouldn't do anything since that
  // item is per definition already the active tab).
  {
    scoped_ptr<ash::ShelfMenuModel> menu(new LauncherApplicationMenuItemModel(
        launcher_controller_->GetApplicationList(item_gmail, 0)));
    // The first element in the menu is a spacing separator. On some systems
    // (e.g. Windows) such things do not exist. As such we check the existence
    // and adjust dynamically.
    int first_item =
        (menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR) ? 1 : 0;
    menu->ActivatedAt(first_item + 3);
  }
  EXPECT_EQ(1, browser()->tab_strip_model()->active_index());

  // Execute the first item.
  {
    scoped_ptr<ash::ShelfMenuModel> menu(new LauncherApplicationMenuItemModel(
        launcher_controller_->GetApplicationList(item_gmail, 0)));
    int first_item =
        (menu->GetTypeAt(0) == ui::MenuModel::TYPE_SEPARATOR) ? 1 : 0;
    menu->ActivatedAt(first_item + 2);
  }
  // Now the active tab should be the second item.
  EXPECT_EQ(0, browser()->tab_strip_model()->active_index());
}

// Checks that the generated menu list properly deletes items.
TEST_F(ChromeLauncherControllerTest, V1AppMenuDeletionExecution) {
  InitLauncherControllerWithBrowser();

  // Add |extension3_| to the launcher and add two items.
  GURL gmail = GURL("https://mail.google.com/mail/u");
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  launcher_controller_->SetRefocusURLPatternForTest(gmail_id, GURL(gmail_url));
  base::string16 title1 = ASCIIToUTF16("Test1");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title1);
  chrome::NewTab(browser());
  base::string16 title2 = ASCIIToUTF16("Test2");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title2);

  // Check that the menu is properly set.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  base::string16 two_menu_items[] = {title1, title2};
  EXPECT_TRUE(CheckMenuCreation(
      launcher_controller_.get(), item_gmail, 2, two_menu_items, false));

  int tabs = browser()->tab_strip_model()->count();
  // Activate the proper tab through the menu item.
  {
    ChromeLauncherAppMenuItems items =
        launcher_controller_->GetApplicationList(item_gmail, 0);
    items[1]->Execute(0);
    EXPECT_EQ(tabs, browser()->tab_strip_model()->count());
  }

  // Delete one tab through the menu item.
  {
    ChromeLauncherAppMenuItems items =
        launcher_controller_->GetApplicationList(item_gmail, 0);
    items[1]->Execute(ui::EF_SHIFT_DOWN);
    EXPECT_EQ(--tabs, browser()->tab_strip_model()->count());
  }
}

// Tests that panels create launcher items correctly
TEST_F(ChromeLauncherControllerTest, AppPanels) {
  InitLauncherControllerWithBrowser();
  // App list and Browser shortcut ShelfItems are added.
  EXPECT_EQ(2, model_observer_->added());

  TestAppIconLoaderImpl* app_icon_loader = new TestAppIconLoaderImpl();
  SetAppIconLoader(app_icon_loader);

  // Test adding an app panel
  std::string app_id = extension1_->id();
  AppWindowLauncherItemController* app_panel_controller =
      new AppWindowLauncherItemController(
          LauncherItemController::TYPE_APP_PANEL,
          "id",
          app_id,
          launcher_controller_.get());
  ash::ShelfID shelf_id1 = launcher_controller_->CreateAppLauncherItem(
      app_panel_controller, app_id, ash::STATUS_RUNNING);
  int panel_index = model_observer_->last_index();
  EXPECT_EQ(3, model_observer_->added());
  EXPECT_EQ(0, model_observer_->changed());
  EXPECT_EQ(1, app_icon_loader->fetch_count());
  model_observer_->clear_counts();

  // App panels should have a separate identifier than the app id
  EXPECT_EQ(0, launcher_controller_->GetShelfIDForAppID(app_id));

  // Setting the app image image should not change the panel if it set its icon
  app_panel_controller->set_image_set_by_controller(true);
  gfx::ImageSkia image;
  launcher_controller_->SetAppImage(app_id, image);
  EXPECT_EQ(0, model_observer_->changed());
  model_observer_->clear_counts();

  // Add a second app panel and verify that it get the same index as the first
  // one had, being added to the left of the existing panel.
  AppWindowLauncherItemController* app_panel_controller2 =
      new AppWindowLauncherItemController(
          LauncherItemController::TYPE_APP_PANEL,
          "id",
          app_id,
          launcher_controller_.get());

  ash::ShelfID shelf_id2 = launcher_controller_->CreateAppLauncherItem(
      app_panel_controller2, app_id, ash::STATUS_RUNNING);
  EXPECT_EQ(panel_index, model_observer_->last_index());
  EXPECT_EQ(1, model_observer_->added());
  model_observer_->clear_counts();

  launcher_controller_->CloseLauncherItem(shelf_id2);
  launcher_controller_->CloseLauncherItem(shelf_id1);
  EXPECT_EQ(2, model_observer_->removed());
}

// Tests that the Gmail extension matches more then the app itself claims with
// the manifest file.
TEST_F(ChromeLauncherControllerTest, GmailMatching) {
  InitLauncherControllerWithBrowser();

  // Create a Gmail browser tab.
  chrome::NewTab(browser());
  base::string16 title = ASCIIToUTF16("Test");
  NavigateAndCommitActiveTabWithTitle(browser(), GURL(gmail_url), title);
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Check that the launcher controller does not recognize the running app.
  EXPECT_FALSE(launcher_controller_->ContentCanBeHandledByGmailApp(content));

  // Installing |extension3_| adds it to the launcher.
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));

  // Check that it is now handled.
  EXPECT_TRUE(launcher_controller_->ContentCanBeHandledByGmailApp(content));

  // Check also that the app has detected that properly.
  ash::ShelfItem item_gmail;
  item_gmail.type = ash::TYPE_APP_SHORTCUT;
  item_gmail.id = gmail_id;
  EXPECT_EQ(2U, launcher_controller_->GetApplicationList(item_gmail, 0).size());
}

// Tests that the Gmail extension does not match the offline verison.
TEST_F(ChromeLauncherControllerTest, GmailOfflineMatching) {
  InitLauncherControllerWithBrowser();

  // Create a Gmail browser tab.
  chrome::NewTab(browser());
  base::string16 title = ASCIIToUTF16("Test");
  NavigateAndCommitActiveTabWithTitle(browser(),
                                      GURL(offline_gmail_url),
                                      title);
  content::WebContents* content =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Installing |extension3_| adds it to the launcher.
  ash::ShelfID gmail_id = model_->next_id();
  extension_service_->AddExtension(extension3_.get());
  EXPECT_EQ(3, model_->item_count());
  int gmail_index = model_->ItemIndexByID(gmail_id);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[gmail_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned(extension3_->id()));

  // The content should not be able to be handled by the app.
  EXPECT_FALSE(launcher_controller_->ContentCanBeHandledByGmailApp(content));
}

// Verify that the launcher item positions are persisted and restored.
TEST_F(ChromeLauncherControllerTest, PersistLauncherItemPositions) {
  InitLauncherController();

  TestAppTabHelperImpl* app_tab_helper = new TestAppTabHelperImpl;
  SetAppTabHelper(app_tab_helper);

  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[1].type);

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(0, tab_strip_model->count());
  chrome::NewTab(browser());
  chrome::NewTab(browser());
  EXPECT_EQ(2, tab_strip_model->count());
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");

  EXPECT_FALSE(launcher_controller_->IsAppPinned("1"));
  launcher_controller_->PinAppWithID("1");
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  launcher_controller_->PinAppWithID("2");

  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[3].type);

  // Move browser shortcut item from index 1 to index 3.
  model_->Move(1, 3);
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[3].type);

  launcher_controller_.reset();
  if (!ash::Shell::HasInstance()) {
    delete item_delegate_manager_;
  } else {
    // Clear already registered ShelfItemDelegate.
    ash::test::ShelfItemDelegateManagerTestAPI test(item_delegate_manager_);
    test.RemoveAllShelfItemDelegateForTest();
  }
  model_.reset(new ash::ShelfModel);

  AddAppListLauncherItem();
  launcher_controller_.reset(
      ChromeLauncherController::CreateInstance(profile(), model_.get()));
  app_tab_helper = new TestAppTabHelperImpl;
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(1), "2");
  SetAppTabHelper(app_tab_helper);
  if (!ash::Shell::HasInstance()) {
    item_delegate_manager_ = new ash::ShelfItemDelegateManager(model_.get());
    SetShelfItemDelegateManager(item_delegate_manager_);
  }
  launcher_controller_->Init();

  // Check ShelfItems are restored after resetting ChromeLauncherController.
  EXPECT_EQ(ash::TYPE_APP_LIST, model_->items()[0].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[1].type);
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[2].type);
  EXPECT_EQ(ash::TYPE_BROWSER_SHORTCUT, model_->items()[3].type);
}

// Verifies pinned apps are persisted and restored.
TEST_F(ChromeLauncherControllerTest, PersistPinned) {
  InitLauncherControllerWithBrowser();
  size_t initial_size = model_->items().size();

  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  EXPECT_EQ(1, tab_strip_model->count());

  TestAppTabHelperImpl* app_tab_helper = new TestAppTabHelperImpl;
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetAppTabHelper(app_tab_helper);

  TestAppIconLoaderImpl* app_icon_loader = new TestAppIconLoaderImpl;
  SetAppIconLoader(app_icon_loader);
  EXPECT_EQ(0, app_icon_loader->fetch_count());

  launcher_controller_->PinAppWithID("1");
  ash::ShelfID id = launcher_controller_->GetShelfIDForAppID("1");
  int app_index = model_->ItemIndexByID(id);
  EXPECT_EQ(1, app_icon_loader->fetch_count());
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[app_index].type);
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_controller_->IsAppPinned("0"));
  EXPECT_EQ(initial_size + 1, model_->items().size());

  launcher_controller_.reset();
  if (!ash::Shell::HasInstance()) {
    delete item_delegate_manager_;
  } else {
    // Clear already registered ShelfItemDelegate.
    ash::test::ShelfItemDelegateManagerTestAPI test(item_delegate_manager_);
    test.RemoveAllShelfItemDelegateForTest();
  }
  model_.reset(new ash::ShelfModel);

  AddAppListLauncherItem();
  launcher_controller_.reset(
      ChromeLauncherController::CreateInstance(profile(), model_.get()));
  app_tab_helper = new TestAppTabHelperImpl;
  app_tab_helper->SetAppID(tab_strip_model->GetWebContentsAt(0), "1");
  SetAppTabHelper(app_tab_helper);
  app_icon_loader = new TestAppIconLoaderImpl;
  SetAppIconLoader(app_icon_loader);
  if (!ash::Shell::HasInstance()) {
    item_delegate_manager_ = new ash::ShelfItemDelegateManager(model_.get());
    SetShelfItemDelegateManager(item_delegate_manager_);
  }
  launcher_controller_->Init();

  EXPECT_EQ(1, app_icon_loader->fetch_count());
  ASSERT_EQ(initial_size + 1, model_->items().size());
  EXPECT_TRUE(launcher_controller_->IsAppPinned("1"));
  EXPECT_FALSE(launcher_controller_->IsAppPinned("0"));
  EXPECT_EQ(ash::TYPE_APP_SHORTCUT, model_->items()[app_index].type);

  launcher_controller_->UnpinAppWithID("1");
  ASSERT_EQ(initial_size, model_->items().size());
}
