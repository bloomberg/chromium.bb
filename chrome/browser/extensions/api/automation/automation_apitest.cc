// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/extensions/api/automation_internal/automation_util.h"
#include "chrome/browser/extensions/chrome_extension_function.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/api/automation_internal.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/ax_event_notification_details.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_node.h"
#include "ui/accessibility/ax_serializable_tree.h"
#include "ui/accessibility/ax_tree.h"
#include "ui/accessibility/ax_tree_serializer.h"
#include "ui/accessibility/tree_generator.h"

namespace extensions {

namespace {
static const char kDomain[] = "a.com";
static const char kSitesDir[] = "automation/sites";
static const char kGotTree[] = "got_tree";
}  // anonymous namespace

class AutomationApiTest : public ExtensionApiTest {
 protected:
  GURL GetURLForPath(const std::string& host, const std::string& path) {
    std::string port = base::IntToString(embedded_test_server()->port());
    GURL::Replacements replacements;
    replacements.SetHostStr(host);
    replacements.SetPortStr(port);
    GURL url =
        embedded_test_server()->GetURL(path).ReplaceComponents(replacements);
    return url;
  }

  void StartEmbeddedTestServer() {
    base::FilePath test_data;
    ASSERT_TRUE(PathService::Get(chrome::DIR_TEST_DATA, &test_data));
    embedded_test_server()->ServeFilesFromDirectory(
        test_data.AppendASCII("extensions/api_test")
        .AppendASCII(kSitesDir));
    ASSERT_TRUE(ExtensionApiTest::StartEmbeddedTestServer());
    host_resolver()->AddRule("*", embedded_test_server()->base_url().host());
  }

  void LoadPage() {
    StartEmbeddedTestServer();
    const GURL url = GetURLForPath(kDomain, "/index.html");
    ui_test_utils::NavigateToURL(browser(), url);
  }

 public:
  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
  }
};

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TestRendererAccessibilityEnabled) {
  LoadPage();

  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  content::WebContents* const tab =
      browser()->tab_strip_model()->GetWebContentsAt(0);
  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_FALSE(tab->IsTreeOnlyAccessibilityModeForTesting());

  base::FilePath extension_path =
      test_data_dir_.AppendASCII("automation/tests/basic");
  ExtensionTestMessageListener got_tree(kGotTree, false /* no reply */);
  LoadExtension(extension_path);
  ASSERT_TRUE(got_tree.WaitUntilSatisfied());

  ASSERT_FALSE(tab->IsFullAccessibilityModeForTesting());
  ASSERT_TRUE(tab->IsTreeOnlyAccessibilityModeForTesting());
}

#if defined(ADDRESS_SANITIZER)
#define Maybe_SanityCheck DISABLED_SanityCheck
#else
#define Maybe_SanityCheck SanityCheck
#endif
IN_PROC_BROWSER_TEST_F(AutomationApiTest, Maybe_SanityCheck) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "sanity_check.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, Unit) {
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/unit", "unit.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, GetTreeByTabId) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "tab_id.html"))
      << message_;
}

#if defined(OS_LINUX) && defined(ADDRESS_SANITIZER)
// Failing on Linux ASan bot: http://crbug.com/391279
#define MAYBE_Events DISABLED_Events
#else
#define MAYBE_Events Events
#endif

IN_PROC_BROWSER_TEST_F(AutomationApiTest, MAYBE_Events) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "events.html"))
      << message_;
}

#if defined(OS_LINUX) && defined(ADDRESS_SANITIZER)
// Timing out on linux ASan bot: http://crbug.com/385701
#define MAYBE_Actions DISABLED_Actions
#else
#define MAYBE_Actions Actions
#endif

IN_PROC_BROWSER_TEST_F(AutomationApiTest, MAYBE_Actions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "actions.html"))
      << message_;
}

#if defined(ADDRESS_SANITIZER)
#define Maybe_Location DISABLED_Location
#else
#define Maybe_Location Location
#endif
IN_PROC_BROWSER_TEST_F(AutomationApiTest, Maybe_Location) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "location.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TabsAutomationBooleanPermissions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest(
          "automation/tests/tabs_automation_boolean", "permissions.html"))
      << message_;
}

// See crbug.com/384673
#if defined(ADDRESS_SANITIZER) || defined(OS_CHROMEOS) || defined(OS_LINUX)
#define Maybe_TabsAutomationBooleanActions DISABLED_TabsAutomationBooleanActions
#else
#define Maybe_TabsAutomationBooleanActions TabsAutomationBooleanActions
#endif
IN_PROC_BROWSER_TEST_F(AutomationApiTest, Maybe_TabsAutomationBooleanActions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest(
          "automation/tests/tabs_automation_boolean", "actions.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, TabsAutomationHostsPermissions) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest(
          "automation/tests/tabs_automation_hosts", "permissions.html"))
      << message_;
}

#if defined(OS_CHROMEOS)
IN_PROC_BROWSER_TEST_F(AutomationApiTest, Desktop) {
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/desktop", "desktop.html"))
      << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopNotRequested) {
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs",
                                  "desktop_not_requested.html")) << message_;
}

IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopActions) {
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/desktop", "actions.html"))
      << message_;
}
#else
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DesktopNotSupported) {
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/desktop",
                                  "desktop_not_supported.html")) << message_;
}
#endif

IN_PROC_BROWSER_TEST_F(AutomationApiTest, CloseTab) {
  StartEmbeddedTestServer();
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/tabs", "close_tab.html"))
      << message_;
}

static const int kPid = 1;
static const int kTab0Rid = 1;
static const int kTab1Rid = 2;

using content::BrowserContext;

typedef ui::AXTreeSerializer<const ui::AXNode*> TreeSerializer;
typedef ui::AXTreeSource<const ui::AXNode*> TreeSource;

#define AX_EVENT_ASSERT_EQUAL ui::AX_EVENT_LOAD_COMPLETE
#define AX_EVENT_ASSERT_NOT_EQUAL ui::AX_EVENT_ACTIVEDESCENDANTCHANGED
#define AX_EVENT_IGNORE ui::AX_EVENT_CHILDREN_CHANGED
#define AX_EVENT_TEST_COMPLETE ui::AX_EVENT_BLUR

// This test is based on ui/accessibility/ax_generated_tree_unittest.cc
// However, because the tree updates need to be sent to the extension, we can't
// use a straightforward set of nested loops as that test does, so this class
// keeps track of where we're up to in our imaginary loops, while the extension
// function classes below do the work of actually incrementing the state when
// appropriate.
// The actual deserialization and comparison happens in the API bindings and the
// test extension respectively: see
// c/t/data/extensions/api_test/automation/tests/generated/generated_trees.js
class TreeSerializationState {
 public:
  TreeSerializationState()
#ifdef NDEBUG
      : tree_size(3),
#else
      : tree_size(2),
#endif
        generator(tree_size),
        num_trees(generator.UniqueTreeCount()),
        tree0_version(0),
        tree1_version(0) {
  }

  // Serializes tree and sends it as an accessibility event to the extension.
  void SendDataForTree(const ui::AXTree* tree,
                       TreeSerializer* serializer,
                       int routing_id,
                       BrowserContext* browser_context) {
    ui::AXTreeUpdate update;
    serializer->SerializeChanges(tree->GetRoot(), &update);
    SendUpdate(update,
               ui::AX_EVENT_LAYOUT_COMPLETE,
               tree->GetRoot()->id(),
               routing_id,
               browser_context);
  }

  // Sends the given AXTreeUpdate to the extension as an accessibility event.
  void SendUpdate(ui::AXTreeUpdate update,
                  ui::AXEvent event,
                  int node_id,
                  int routing_id,
                  BrowserContext* browser_context) {
    content::AXEventNotificationDetails detail(update.node_id_to_clear,
                                               update.nodes,
                                               event,
                                               node_id,
                                               kPid,
                                               routing_id);
    std::vector<content::AXEventNotificationDetails> details;
    details.push_back(detail);
    automation_util::DispatchAccessibilityEventsToAutomation(details,
                                                             browser_context);
  }

  // Notify the extension bindings to destroy the tree for the given tab
  // (identified by routing_id)
  void SendTreeDestroyedEvent(int routing_id, BrowserContext* browser_context) {
    automation_util::DispatchTreeDestroyedEventToAutomation(
        kPid, routing_id, browser_context);
  }

  // Reset tree0 to a new generated tree based on tree0_version, reset
  // tree0_source accordingly.
  void ResetTree0() {
    tree0.reset(new ui::AXSerializableTree);
    tree0_source.reset(tree0->CreateTreeSource());
    generator.BuildUniqueTree(tree0_version, tree0.get());
    if (!serializer0.get())
      serializer0.reset(new TreeSerializer(tree0_source.get()));
  }

  // Reset tree0, set up serializer0, send down the initial tree data to create
  // the tree in the extension
  void InitializeTree0(BrowserContext* browser_context) {
    ResetTree0();
    serializer0->ChangeTreeSourceForTesting(tree0_source.get());
    serializer0->Reset();
    SendDataForTree(tree0.get(), serializer0.get(), kTab0Rid, browser_context);
  }

  // Reset tree1 to a new generated tree based on tree1_version, reset
  // tree1_source accordingly.
  void ResetTree1() {
    tree1.reset(new ui::AXSerializableTree);
    tree1_source.reset(tree1->CreateTreeSource());
    generator.BuildUniqueTree(tree1_version, tree1.get());
    if (!serializer1.get())
      serializer1.reset(new TreeSerializer(tree1_source.get()));
  }

  // Reset tree1, set up serializer1, send down the initial tree data to create
  // the tree in the extension
  void InitializeTree1(BrowserContext* browser_context) {
    ResetTree1();
    serializer1->ChangeTreeSourceForTesting(tree1_source.get());
    serializer1->Reset();
    SendDataForTree(tree1.get(), serializer1.get(), kTab1Rid, browser_context);
  }

  const int tree_size;
  const ui::TreeGenerator generator;

  // The loop variables: comments indicate which variables in
  // ax_generated_tree_unittest they correspond to.
  const int num_trees; // n
  int tree0_version;   // i
  int tree1_version;   // j
  int starting_node;   // k

  // Tree infrastructure; tree0 and tree1 need to be regenerated whenever
  // tree0_version and tree1_version change, respectively; tree0_source and
  // tree1_source need to be reset whenever that happens.
  scoped_ptr<ui::AXSerializableTree> tree0, tree1;
  scoped_ptr<TreeSource> tree0_source, tree1_source;
  scoped_ptr<TreeSerializer> serializer0, serializer1;

  // Whether tree0 needs to be destroyed after the extension has performed its
  // checks
  bool destroy_tree0;
};

static TreeSerializationState state;

// Override for chrome.automationInternal.enableTab
// This fakes out the process and routing IDs for two "tabs", which contain the
// source and target trees, respectively, and sends down the current tree for
// the requested tab - tab 1 always has tree1, and tab 0 starts with tree0
// and then has a series of updates intended to translate tree0 to tree1.
// Once all the updates have been sent, the extension asserts that both trees
// are equivalent, and then one or both of the trees are reset to a new version.
class FakeAutomationInternalEnableTabFunction
    : public UIThreadExtensionFunction {
 public:
  FakeAutomationInternalEnableTabFunction() {}

  ExtensionFunction::ResponseAction Run() OVERRIDE {
    using api::automation_internal::EnableTab::Params;
    scoped_ptr<Params> params(Params::Create(*args_));
    EXTENSION_FUNCTION_VALIDATE(params.get());
    if (!params->tab_id.get())
      return RespondNow(Error("tab_id not specified"));
    int tab_id = *params->tab_id;
    if (tab_id == 0) {
      // tab 0 <--> tree0
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&TreeSerializationState::InitializeTree0,
                     base::Unretained(&state),
                     base::Unretained(browser_context())));
      return RespondNow(
          ArgumentList(api::automation_internal::EnableTab::Results::Create(
              kPid, kTab0Rid)));
    }
    if (tab_id == 1) {
      // tab 1 <--> tree1
      base::MessageLoop::current()->PostTask(
          FROM_HERE,
          base::Bind(&TreeSerializationState::InitializeTree1,
                     base::Unretained(&state),
                     base::Unretained(browser_context())));
      return RespondNow(
          ArgumentList(api::automation_internal::EnableTab::Results::Create(
              kPid, kTab1Rid)));
    }
    return RespondNow(Error("Unrecognised tab_id"));
  }
};

// Factory method for use in OverrideFunction()
ExtensionFunction* FakeAutomationInternalEnableTabFunctionFactory() {
  return new FakeAutomationInternalEnableTabFunction();
}

// Helper method to serialize a series of updates via source_serializer to
// transform the tree which source_serializer was initialized from into
// target_tree, and then trigger the test code to assert the two tabs contain
// the same tree.
void TransformTree(TreeSerializer* source_serializer,
                   ui::AXTree* target_tree,
                   TreeSource* target_tree_source,
                   content::BrowserContext* browser_context) {
  source_serializer->ChangeTreeSourceForTesting(target_tree_source);
  for (int node_delta = 0; node_delta < state.tree_size; ++node_delta) {
    int id = 1 + (state.starting_node + node_delta) % state.tree_size;
    ui::AXTreeUpdate update;
    source_serializer->SerializeChanges(target_tree->GetFromId(id), &update);
    bool is_last_update = node_delta == state.tree_size - 1;
    ui::AXEvent event =
        is_last_update ? AX_EVENT_ASSERT_EQUAL : AX_EVENT_IGNORE;
    state.SendUpdate(
        update, event, target_tree->GetRoot()->id(), kTab0Rid, browser_context);
  }
}

// Helper method to send a no-op tree update to tab 0 with the given event.
void SendEvent(ui::AXEvent event, content::BrowserContext* browser_context) {
  ui::AXTreeUpdate update;
  ui::AXNode* root = state.tree0->GetRoot();
  state.serializer0->SerializeChanges(root, &update);
  state.SendUpdate(update, event, root->id(), kTab0Rid, browser_context);
}

// Override for chrome.automationInternal.performAction
// This is used as a synchronization mechanism; the general flow is:
// 1. The extension requests tree0 and tree1 (on tab 0 and tab 1 respectively)
// 2. FakeAutomationInternalEnableTabFunction sends down the trees
// 3. When the callback for getTree(0) fires, the extension calls doDefault() on
//    the root node of tree0, which calls into this class's Run() method.
// 4. In the normal case, we're in the "inner loop" (iterating over
//    starting_node). For each value of starting_node, we do the following:
//    a. Serialize a sequence of updates which should transform tree0 into
//       tree1. Each of these updates is sent as a childrenChanged event,
//       except for the last which is sent as a loadComplete event.
//    b. state.destroy_tree0 is set to true
//    c. state.starting_node gets incremented
//    d. The loadComplete event triggers an assertion in the extension.
//    e. The extension performs another doDefault() on the root node of the
//       tree.
//    f. This time, we send a destroy event to tab0, so that the tree can be
//       reset.
//    g. The extension is notified of the tree's destruction and requests the
//       tree for tab 0 again, returning to step 2.
// 5. When starting_node exceeds state.tree_size, we increment tree0_version if
//    it would not exceed state.num_trees, or increment tree1_version and reset
//    tree0_version to 0 otherwise, and reset starting_node to 0.
//    Then we reset one or both trees as appropriate, and send down destroyed
//    events similarly, causing the extension to re-request the tree and going
//    back to step 2 again.
// 6. When tree1_version has gone through all possible values, we send a blur
//    event, signaling the extension to call chrome.test.succeed() and finish
//    the test.
class FakeAutomationInternalPerformActionFunction
    : public UIThreadExtensionFunction {
 public:
  FakeAutomationInternalPerformActionFunction() {}

  ExtensionFunction::ResponseAction Run() OVERRIDE {
    if (state.destroy_tree0) {
      // Step 4.f: tell the extension to destroy the tree and re-request it.
      state.SendTreeDestroyedEvent(kTab0Rid, browser_context());
      state.destroy_tree0 = false;
      return RespondNow(NoArguments());
    }

    TreeSerializer* serializer0 = state.serializer0.get();
    if (state.starting_node < state.tree_size) {
      // As a sanity check, if the trees are not equal, assert that they are not
      // equal before serializing changes.
      if (state.tree0_version != state.tree1_version)
        SendEvent(AX_EVENT_ASSERT_NOT_EQUAL, browser_context());

      // Step 4.a: pretend that tree0 turned into tree1, and serialize
      // a sequence of updates to tab 0 to match.
      TransformTree(serializer0,
                    state.tree1.get(),
                    state.tree1_source.get(),
                    browser_context());

      // Step 4.b: remember that we need to tell the extension to destroy and
      // re-request the tree on the next action.
      state.destroy_tree0 = true;

      // Step 4.c: increment starting_node.
      state.starting_node++;
    } else if (state.tree0_version < state.num_trees - 1) {
      // Step 5: Increment tree0_version and reset starting_node
      state.tree0_version++;
      state.starting_node = 0;

      // Step 5: Reset tree0 and tell the extension to destroy and re-request it
      state.SendTreeDestroyedEvent(kTab0Rid, browser_context());
    } else if (state.tree1_version < state.num_trees - 1) {
      // Step 5: Increment tree1_version and reset tree0_version and
      // starting_node
      state.tree1_version++;
      state.tree0_version = 0;
      state.starting_node = 0;

      // Step 5: Reset tree0 and tell the extension to destroy and re-request it
      state.SendTreeDestroyedEvent(kTab0Rid, browser_context());

      // Step 5: Reset tree1 and tell the extension to destroy and re-request it
      state.SendTreeDestroyedEvent(kTab1Rid, browser_context());
    } else {
      // Step 6: Send a TEST_COMPLETE (blur) event to signal the extension to
      // call chrome.test.succeed().
      SendEvent(AX_EVENT_TEST_COMPLETE, browser_context());
    }

    return RespondNow(NoArguments());
  }
};

// Factory method for use in OverrideFunction()
ExtensionFunction* FakeAutomationInternalPerformActionFunctionFactory() {
  return new FakeAutomationInternalPerformActionFunction();
}

// http://crbug.com/396353
IN_PROC_BROWSER_TEST_F(AutomationApiTest, DISABLED_GeneratedTree) {
  ASSERT_TRUE(extensions::ExtensionFunctionDispatcher::OverrideFunction(
      "automationInternal.enableTab",
      FakeAutomationInternalEnableTabFunctionFactory));
  ASSERT_TRUE(extensions::ExtensionFunctionDispatcher::OverrideFunction(
      "automationInternal.performAction",
      FakeAutomationInternalPerformActionFunctionFactory));
  ASSERT_TRUE(RunExtensionSubtest("automation/tests/generated",
                                  "generated_trees.html")) << message_;
}

}  // namespace extensions
