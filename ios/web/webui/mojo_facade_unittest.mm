// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/webui/mojo_facade.h"

#include <memory>

#import "base/mac/scoped_nsobject.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "ios/web/public/test/web_test.h"
#import "ios/web/public/web_state/js/crw_js_injection_evaluator.h"
#include "ios/web/test/mojo_test.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/service_manager/public/cpp/identity.h"
#include "services/service_manager/public/cpp/interface_factory.h"
#include "services/service_manager/public/cpp/interface_registry.h"
#import "testing/gtest_mac.h"
#import "third_party/ocmock/OCMock/OCMock.h"

namespace web {

namespace {

// Serializes the given |object| to JSON string.
std::string GetJson(id object) {
  NSData* json_as_data =
      [NSJSONSerialization dataWithJSONObject:object options:0 error:nil];
  base::scoped_nsobject<NSString> json_as_string([[NSString alloc]
      initWithData:json_as_data
          encoding:NSUTF8StringEncoding]);
  return base::SysNSStringToUTF8(json_as_string);
}

// Deserializes the given |json| to an object.
id GetObject(const std::string& json) {
  NSData* json_as_data =
      [base::SysUTF8ToNSString(json) dataUsingEncoding:NSUTF8StringEncoding];
  return [NSJSONSerialization JSONObjectWithData:json_as_data
                                         options:0
                                           error:nil];
}

// Test mojo handler factory.
class TestUIHandlerFactory
    : public service_manager::InterfaceFactory<TestUIHandlerMojo> {
 public:
  ~TestUIHandlerFactory() override {}

 private:
  // service_manager::InterfaceFactory overrides.
  void Create(const service_manager::Identity& remote_identity,
              mojo::InterfaceRequest<TestUIHandlerMojo> request) override {}
};

}  // namespace

// A test fixture to test MojoFacade class.
class MojoFacadeTest : public WebTest {
 protected:
  MojoFacadeTest() {
    interface_registry_.reset(
        new service_manager::InterfaceRegistry(std::string()));
    interface_registry_->AddInterface(&ui_handler_factory_);
    evaluator_.reset([[OCMockObject
        mockForProtocol:@protocol(CRWJSInjectionEvaluator)] retain]);
    facade_.reset(new MojoFacade(
        interface_registry_.get(),
        static_cast<id<CRWJSInjectionEvaluator>>(evaluator_.get())));
  }

  OCMockObject* evaluator() { return evaluator_.get(); }
  MojoFacade* facade() { return facade_.get(); }

 private:
  TestUIHandlerFactory ui_handler_factory_;
  std::unique_ptr<service_manager::InterfaceRegistry> interface_registry_;
  base::scoped_nsobject<OCMockObject> evaluator_;
  std::unique_ptr<MojoFacade> facade_;
};

// Tests connecting to existing interface and closing the handle.
TEST_F(MojoFacadeTest, GetInterfaceAndCloseHandle) {
  // Bind to the interface.
  NSDictionary* connect = @{
    @"name" : @"interface_provider.getInterface",
    @"args" : @{
      @"interfaceName" : @"::TestUIHandlerMojo",
    },
  };

  std::string handle_as_string = facade()->HandleMojoMessage(GetJson(connect));
  EXPECT_FALSE(handle_as_string.empty());
  int handle = 0;
  EXPECT_TRUE(base::StringToInt(handle_as_string, &handle));

  // Close the handle.
  NSDictionary* close = @{
    @"name" : @"core.close",
    @"args" : @{
      @"handle" : @(handle),
    },
  };
  std::string result_as_string = facade()->HandleMojoMessage(GetJson(close));
  EXPECT_FALSE(result_as_string.empty());
  int result = 0;
  EXPECT_TRUE(base::StringToInt(result_as_string, &result));
  EXPECT_EQ(MOJO_RESULT_OK, static_cast<MojoResult>(result));
}

// Tests creating a message pipe without options.
TEST_F(MojoFacadeTest, CreateMessagePipeWithoutOptions) {
  // Create a message pipe.
  NSDictionary* create = @{
    @"name" : @"core.createMessagePipe",
    @"args" : @{
      @"optionsDict" : [NSNull null],
    },
  };
  std::string response_as_string = facade()->HandleMojoMessage(GetJson(create));

  // Verify handles.
  EXPECT_FALSE(response_as_string.empty());
  NSDictionary* response_as_dict = GetObject(response_as_string);
  EXPECT_TRUE([response_as_dict isKindOfClass:[NSDictionary class]]);
  id handle0 = response_as_dict[@"handle0"];
  EXPECT_TRUE(handle0);
  id handle1 = response_as_dict[@"handle1"];
  EXPECT_TRUE(handle1);

  // Close handle0.
  NSDictionary* close0 = @{
    @"name" : @"core.close",
    @"args" : @{
      @"handle" : handle0,
    },
  };
  std::string result0_as_string = facade()->HandleMojoMessage(GetJson(close0));
  EXPECT_FALSE(result0_as_string.empty());
  int result0 = 0;
  EXPECT_TRUE(base::StringToInt(result0_as_string, &result0));
  EXPECT_EQ(MOJO_RESULT_OK, static_cast<MojoResult>(result0));

  // Close handle1.
  NSDictionary* close1 = @{
    @"name" : @"core.close",
    @"args" : @{
      @"handle" : handle1,
    },
  };
  std::string result1_as_string = facade()->HandleMojoMessage(GetJson(close1));
  EXPECT_FALSE(result1_as_string.empty());
  int result1 = 0;
  EXPECT_TRUE(base::StringToInt(result1_as_string, &result1));
  EXPECT_EQ(MOJO_RESULT_OK, static_cast<MojoResult>(result1));
}

// Tests watching the pipe.
TEST_F(MojoFacadeTest, Watch) {
  // Create a message pipe.
  NSDictionary* create = @{
    @"name" : @"core.createMessagePipe",
    @"args" : @{
      @"optionsDict" : [NSNull null],
    },
  };
  std::string response_as_string = facade()->HandleMojoMessage(GetJson(create));

  // Verify handles.
  EXPECT_FALSE(response_as_string.empty());
  NSDictionary* response_as_dict = GetObject(response_as_string);
  EXPECT_TRUE([response_as_dict isKindOfClass:[NSDictionary class]]);
  id handle0 = response_as_dict[@"handle0"];
  EXPECT_TRUE(handle0);
  id handle1 = response_as_dict[@"handle1"];
  EXPECT_TRUE(handle1);

  // Start watching one end of the pipe.
  int callback_id = 99;
  NSDictionary* watch = @{
    @"name" : @"support.watch",
    @"args" : @{
      @"handle" : handle0,
      @"signals" : @(MOJO_HANDLE_SIGNAL_READABLE),
      @"callbackId" : @(callback_id),
    },
  };
  std::string watch_id_as_string = facade()->HandleMojoMessage(GetJson(watch));
  EXPECT_FALSE(watch_id_as_string.empty());
  int watch_id = 0;
  EXPECT_TRUE(base::StringToInt(watch_id_as_string, &watch_id));

  // Start waiting for the watch callback.
  __block bool callback_received = false;
  NSString* expected_script =
      [NSString stringWithFormat:@"__crWeb.mojo.signalWatch(%d, %d)",
                                 callback_id, MOJO_RESULT_OK];
  [[[evaluator() expect] andDo:^(NSInvocation*) {
    callback_received = true;
  }] executeJavaScript:expected_script completionHandler:nil];

  // Write to the other end of the pipe.
  NSDictionary* write = @{
    @"name" : @"core.writeMessage",
    @"args" : @{
      @"handle" : handle1,
      @"handles" : @[],
      @"flags" : @(MOJO_WRITE_MESSAGE_FLAG_NONE),
      @"buffer" : @{@"0" : @0}
    },
  };
  std::string result_as_string = facade()->HandleMojoMessage(GetJson(write));
  EXPECT_FALSE(result_as_string.empty());
  int result = 0;
  EXPECT_TRUE(base::StringToInt(result_as_string, &result));
  EXPECT_EQ(MOJO_RESULT_OK, static_cast<MojoResult>(result));

  base::test::ios::WaitUntilCondition(
      ^{
        return callback_received;
      },
      true, base::TimeDelta());
}

// Tests reading the message from the pipe.
TEST_F(MojoFacadeTest, ReadWrite) {
  // Create a message pipe.
  NSDictionary* create = @{
    @"name" : @"core.createMessagePipe",
    @"args" : @{
      @"optionsDict" : [NSNull null],
    },
  };
  std::string response_as_string = facade()->HandleMojoMessage(GetJson(create));

  // Verify handles.
  EXPECT_FALSE(response_as_string.empty());
  NSDictionary* response_as_dict = GetObject(response_as_string);
  EXPECT_TRUE([response_as_dict isKindOfClass:[NSDictionary class]]);
  id handle0 = response_as_dict[@"handle0"];
  EXPECT_TRUE(handle0);
  id handle1 = response_as_dict[@"handle1"];
  EXPECT_TRUE(handle1);

  // Write to the other end of the pipe.
  NSDictionary* write = @{
    @"name" : @"core.writeMessage",
    @"args" : @{
      @"handle" : handle1,
      @"handles" : @[],
      @"flags" : @(MOJO_WRITE_MESSAGE_FLAG_NONE),
      @"buffer" : @{@"0" : @9, @"1" : @2, @"2" : @2008}
    },
  };
  std::string result_as_string = facade()->HandleMojoMessage(GetJson(write));
  EXPECT_FALSE(result_as_string.empty());
  int result = 0;
  EXPECT_TRUE(base::StringToInt(result_as_string, &result));
  EXPECT_EQ(MOJO_RESULT_OK, static_cast<MojoResult>(result));

  // Read the message from the pipe.
  NSDictionary* read = @{
    @"name" : @"core.readMessage",
    @"args" : @{
      @"handle" : handle0,
      @"flags" : @(MOJO_READ_MESSAGE_FLAG_NONE),
    },
  };
  NSDictionary* message = GetObject(facade()->HandleMojoMessage(GetJson(read)));
  EXPECT_TRUE([message isKindOfClass:[NSDictionary class]]);
  EXPECT_TRUE(message);
  NSArray* expected_message = @[ @9, @2, @216 ];  // 2008 does not fit 8-bit.
  EXPECT_NSEQ(expected_message, message[@"buffer"]);
  EXPECT_FALSE([message[@"handles"] count]);
  EXPECT_EQ(MOJO_RESULT_OK, [message[@"result"] unsignedIntValue]);
}

}  // namespace web
