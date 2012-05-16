// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "content/browser/renderer_host/render_widget_host_view_mac_editcommand_helper.h"

#import <Cocoa/Cocoa.h>

#include "base/message_loop.h"
#include "content/browser/renderer_host/render_widget_host_delegate.h"
#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/common/view_messages.h"
#include "content/test/mock_render_process_host.h"
#include "content/test/test_browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using content::MockRenderProcessHost;
using content::RenderWidgetHostImpl;

class RenderWidgetHostViewMacEditCommandHelperTest : public PlatformTest {
};

// Bare bones obj-c class for testing purposes.
@interface RenderWidgetHostViewMacEditCommandHelperTestClass : NSObject
@end

@implementation RenderWidgetHostViewMacEditCommandHelperTestClass
@end

// Class that owns a RenderWidgetHostViewMac.
@interface RenderWidgetHostViewMacOwner :
    NSObject<RenderWidgetHostViewMacOwner> {
  RenderWidgetHostViewMac* rwhvm_;
}

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)rwhvm;
@end

@implementation RenderWidgetHostViewMacOwner

- (id)initWithRenderWidgetHostViewMac:(RenderWidgetHostViewMac*)rwhvm {
  if ((self = [super init])) {
    rwhvm_ = rwhvm;
  }
  return self;
}

- (RenderWidgetHostViewMac*)renderWidgetHostViewMac {
  return rwhvm_;
}

@end


namespace {
  // Returns true if all the edit command names in the array are present
  // in test_obj.
  // edit_commands is a list of NSStrings, selector names are formed by
  // appending a trailing ':' to the string.
  bool CheckObjectRespondsToEditCommands(NSArray* edit_commands,
                                         id test_obj) {
    for (NSString* edit_command_name in edit_commands) {
      NSString* sel_str = [edit_command_name stringByAppendingString:@":"];
      if (![test_obj respondsToSelector:NSSelectorFromString(sel_str)]) {
        return false;
      }
    }

    return true;
  }
}  // namespace

class MockRenderWidgetHostDelegate : public content::RenderWidgetHostDelegate {
 public:
  MockRenderWidgetHostDelegate() {}
  virtual ~MockRenderWidgetHostDelegate() {}
};

// Create a RenderWidget for which we can filter messages.
class RenderWidgetHostEditCommandCounter : public RenderWidgetHostImpl {
 public:
  RenderWidgetHostEditCommandCounter(
      content::RenderWidgetHostDelegate* delegate,
      content::RenderProcessHost* process,
      int routing_id)
    : RenderWidgetHostImpl(delegate, process, routing_id),
      edit_command_message_count_(0) {
  }

  virtual bool Send(IPC::Message* message) {
    if (message->type() == ViewMsg_ExecuteEditCommand::ID)
      edit_command_message_count_++;
    return RenderWidgetHostImpl::Send(message);
  }

  unsigned int edit_command_message_count_;
};


// Tests that editing commands make it through the pipeline all the way to
// RenderWidgetHost.
// Disabled, http://crbug.com/93286.
TEST_F(RenderWidgetHostViewMacEditCommandHelperTest,
       DISABLED_TestEditingCommandDelivery) {
  RenderWidgetHostViewMacEditCommandHelper helper;
  NSArray* edit_command_strings = helper.GetEditSelectorNames();

  // Set up a mock render widget and set expectations.
  MessageLoopForUI message_loop;
  TestBrowserContext browser_context;
  MockRenderProcessHost mock_process(&browser_context);
  MockRenderWidgetHostDelegate delegate;
  RenderWidgetHostEditCommandCounter render_widget(&delegate, &mock_process, 0);

  // RenderWidgetHostViewMac self destructs (RenderWidgetHostViewMacCocoa
  // takes ownership) so no need to delete it ourselves.
  RenderWidgetHostViewMac* rwhvm = static_cast<RenderWidgetHostViewMac*>(
      content::RenderWidgetHostView::CreateViewForWidget(&render_widget));

  RenderWidgetHostViewMacOwner* rwhwvm_owner =
      [[[RenderWidgetHostViewMacOwner alloc]
          initWithRenderWidgetHostViewMac:rwhvm] autorelease];

  helper.AddEditingSelectorsToClass([rwhwvm_owner class]);

  for (NSString* edit_command_name in edit_command_strings) {
    NSString* sel_str = [edit_command_name stringByAppendingString:@":"];
    [rwhwvm_owner performSelector:NSSelectorFromString(sel_str) withObject:nil];
  }

  size_t num_edit_commands = [edit_command_strings count];
  EXPECT_EQ(render_widget.edit_command_message_count_, num_edit_commands);
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
  RenderWidgetHostViewMacOwner* rwhvm_owner =
      [[[RenderWidgetHostViewMacOwner alloc] init] autorelease];

  // The select all menu should always be enabled.
  SEL select_all = NSSelectorFromString(@"selectAll:");
  ASSERT_TRUE(helper.IsMenuItemEnabled(select_all, rwhvm_owner));

  // Random selectors should be enabled by the function.
  SEL garbage_selector = NSSelectorFromString(@"randomGarbageSelector:");
  ASSERT_FALSE(helper.IsMenuItemEnabled(garbage_selector, rwhvm_owner));

  // TODO(jeremy): Currently IsMenuItemEnabled just returns true for all edit
  // selectors.  Once we go past that we should do more extensive testing here.
}
