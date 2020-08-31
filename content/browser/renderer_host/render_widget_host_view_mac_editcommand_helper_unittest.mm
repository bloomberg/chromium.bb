// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"

#import <Cocoa/Cocoa.h>
#include <stddef.h>
#include <stdint.h>

#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/browser/compositor/test/test_image_transport_factory.h"
#include "content/browser/gpu/compositor_util.h"
#include "content/browser/renderer_host/frame_token_message_queue.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/input_messages.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_context.h"
#include "content/test/mock_widget_impl.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"
#include "ui/accelerated_widget_mac/window_resize_helper_mac.h"
#include "ui/base/layout.h"

using content::RenderWidgetHostViewMac;

// Bare bones obj-c class for testing purposes.
@interface RenderWidgetHostViewMacEditCommandHelperTestClass : NSObject
@end

@implementation RenderWidgetHostViewMacEditCommandHelperTestClass
@end

// Class that owns a RenderWidgetHostViewMac.
@interface RenderWidgetHostNSViewHostOwner
    : NSObject <RenderWidgetHostNSViewHostOwner> {
  RenderWidgetHostViewMac* _rwhvm;
}

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)rwhvm;
@end

@implementation RenderWidgetHostNSViewHostOwner

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)rwhvm {
  if ((self = [super init])) {
    _rwhvm = rwhvm;
  }
  return self;
}

- (remote_cocoa::mojom::RenderWidgetHostNSViewHost*)renderWidgetHostNSViewHost {
  return _rwhvm;
}

@end

namespace content {
namespace {

// Returns true if all the edit command names in the array are present in
// test_obj.  edit_commands is a list of NSStrings, selector names are formed
// by appending a trailing ':' to the string.
bool CheckObjectRespondsToEditCommands(NSArray* edit_commands, id test_obj) {
  for (NSString* edit_command_name in edit_commands) {
    NSString* sel_str = [edit_command_name stringByAppendingString:@":"];
    if (![test_obj respondsToSelector:NSSelectorFromString(sel_str)]) {
      return false;
    }
  }
  return true;
}

class RenderWidgetHostDelegateEditCommandCounter
    : public RenderWidgetHostDelegate {
 public:
  RenderWidgetHostDelegateEditCommandCounter()
      : edit_command_message_count_(0) {}
  ~RenderWidgetHostDelegateEditCommandCounter() override {}
  unsigned int edit_command_message_count_;

 private:
  void ExecuteEditCommand(
      const std::string& command,
      const base::Optional<base::string16>& value) override {
    edit_command_message_count_++;
  }
  void Undo() override {}
  void Redo() override {}
  void Cut() override {}
  void Copy() override {}
  void Paste() override {}
  void PasteAndMatchStyle() override {}
  void SelectAll() override {}
};

class RenderWidgetHostViewMacEditCommandHelperTest : public PlatformTest {
 protected:
  void SetUp() override {
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
  }
  void TearDown() override { ImageTransportFactory::Terminate(); }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
};

class RenderWidgetHostViewMacEditCommandHelperWithTaskEnvTest
    : public PlatformTest {
 protected:
  void SetUp() override {
    ImageTransportFactory::SetFactory(
        std::make_unique<TestImageTransportFactory>());
  }
  void TearDown() override { ImageTransportFactory::Terminate(); }

 private:
  // This has a MessageLoop for ImageTransportFactory and enables
  // BrowserThread::UI for RecyclableCompositorMac used by
  // RenderWidgetHostViewMac.
  content::BrowserTaskEnvironment task_environment_;
};

}  // namespace

// Tests that editing commands make it through the pipeline all the way to
// RenderWidgetHost.
TEST_F(RenderWidgetHostViewMacEditCommandHelperWithTaskEnvTest,
       TestEditingCommandDelivery) {
  RenderWidgetHostDelegateEditCommandCounter delegate;
  TestBrowserContext browser_context;
  MockRenderProcessHostFactory process_host_factory;
  RenderProcessHost* process_host =
      process_host_factory.CreateRenderProcessHost(&browser_context, nullptr);

  // Populates |g_supported_scale_factors|.
  std::vector<ui::ScaleFactor> supported_factors;
  supported_factors.push_back(ui::SCALE_FACTOR_100P);
  ui::test::ScopedSetSupportedScaleFactors scoped_supported(supported_factors);

  @autoreleasepool {
    int32_t routing_id = process_host->GetNextRoutingID();
    mojo::PendingRemote<mojom::Widget> widget;
    std::unique_ptr<MockWidgetImpl> widget_impl =
        std::make_unique<MockWidgetImpl>(
            widget.InitWithNewPipeAndPassReceiver());

    RenderWidgetHostImpl* render_widget = new RenderWidgetHostImpl(
        &delegate, process_host, routing_id, std::move(widget),
        /*hidden=*/false, std::make_unique<FrameTokenMessageQueue>());

    ui::WindowResizeHelperMac::Get()->Init(base::ThreadTaskRunnerHandle::Get());

    // Owned by its |GetInProcessNSView()|, i.e. |rwhv_cocoa|.
    RenderWidgetHostViewMac* rwhv_mac =
        new RenderWidgetHostViewMac(render_widget);
    base::scoped_nsobject<RenderWidgetHostViewCocoa> rwhv_cocoa(
        [rwhv_mac->GetInProcessNSView() retain]);

    RenderWidgetHostViewMacEditCommandHelper helper;
    NSArray* edit_command_strings = helper.GetEditSelectorNames();
    RenderWidgetHostNSViewHostOwner* rwhwvm_owner =
        [[[RenderWidgetHostNSViewHostOwner alloc]
            initWithRenderWidgetHostViewMac:rwhv_mac] autorelease];

    helper.AddEditingSelectorsToClass([rwhwvm_owner class]);

    for (NSString* edit_command_name in edit_command_strings) {
      NSString* sel_str = [edit_command_name stringByAppendingString:@":"];
      [rwhwvm_owner performSelector:NSSelectorFromString(sel_str)
                         withObject:nil];
    }

    size_t num_edit_commands = [edit_command_strings count];
    EXPECT_EQ(delegate.edit_command_message_count_, num_edit_commands);
    rwhv_cocoa.reset();

    // The |render_widget|'s process needs to be deleted within |message_loop|.
    delete render_widget;
  }

  ui::WindowResizeHelperMac::Get()->ShutdownForTests();
}

// Test RenderWidgetHostViewMacEditCommandHelper::AddEditingSelectorsToClass
TEST_F(RenderWidgetHostViewMacEditCommandHelperTest,
       TestAddEditingSelectorsToClass) {
  RenderWidgetHostViewMacEditCommandHelper helper;
  NSArray* edit_command_strings = helper.GetEditSelectorNames();
  ASSERT_GT([edit_command_strings count], 0U);

  // Create a class instance and add methods to the class.
  RenderWidgetHostViewMacEditCommandHelperTestClass* test_obj =
      [[[RenderWidgetHostViewMacEditCommandHelperTestClass alloc] init]
          autorelease];

  // Check that edit commands aren't already attached to the object.
  ASSERT_FALSE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));

  helper.AddEditingSelectorsToClass([test_obj class]);

  // Check that all edit commands where added.
  ASSERT_TRUE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));

  // AddEditingSelectorsToClass() should be idempotent.
  helper.AddEditingSelectorsToClass([test_obj class]);

  // Check that all edit commands are still there.
  ASSERT_TRUE(CheckObjectRespondsToEditCommands(edit_command_strings,
      test_obj));
}

// Test RenderWidgetHostViewMacEditCommandHelper::IsMenuItemEnabled.
TEST_F(RenderWidgetHostViewMacEditCommandHelperTest, TestMenuItemEnabling) {
  RenderWidgetHostViewMacEditCommandHelper helper;
  RenderWidgetHostNSViewHostOwner* rwhvm_owner =
      [[[RenderWidgetHostNSViewHostOwner alloc] init] autorelease];

  // The select all menu should always be enabled.
  SEL select_all = NSSelectorFromString(@"selectAll:");
  ASSERT_TRUE(helper.IsMenuItemEnabled(select_all, rwhvm_owner));

  // Random selectors should be enabled by the function.
  SEL garbage_selector = NSSelectorFromString(@"randomGarbageSelector:");
  ASSERT_FALSE(helper.IsMenuItemEnabled(garbage_selector, rwhvm_owner));

  // TODO(jeremy): Currently IsMenuItemEnabled just returns true for all edit
  // selectors.  Once we go past that we should do more extensive testing here.
}

}  // namespace content
