// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_hooks_delegate.h"

#include "base/strings/stringprintf.h"
#include "extensions/common/extension_builder.h"
#include "extensions/common/value_builder.h"
#include "extensions/renderer/bindings/api_binding_test_util.h"
#include "extensions/renderer/message_target.h"
#include "extensions/renderer/messaging_util.h"
#include "extensions/renderer/native_extension_bindings_system.h"
#include "extensions/renderer/native_extension_bindings_system_test_base.h"
#include "extensions/renderer/native_renderer_messaging_service.h"
#include "extensions/renderer/runtime_hooks_delegate.h"
#include "extensions/renderer/script_context.h"
#include "extensions/renderer/send_message_tester.h"

namespace extensions {

class ExtensionHooksDelegateTest
    : public NativeExtensionBindingsSystemUnittest {
 public:
  ExtensionHooksDelegateTest() {}
  ~ExtensionHooksDelegateTest() override {}

  // NativeExtensionBindingsSystemUnittest:
  void SetUp() override {
    NativeExtensionBindingsSystemUnittest::SetUp();
    messaging_service_ =
        std::make_unique<NativeRendererMessagingService>(bindings_system());

    bindings_system()
        ->api_system()
        ->GetHooksForAPI("extension")
        ->SetDelegate(
            std::make_unique<ExtensionHooksDelegate>(messaging_service_.get()));
    bindings_system()->api_system()->GetHooksForAPI("runtime")->SetDelegate(
        std::make_unique<RuntimeHooksDelegate>(messaging_service_.get()));

    scoped_refptr<Extension> mutable_extension = BuildExtension();
    RegisterExtension(mutable_extension);
    extension_ = mutable_extension;

    v8::HandleScope handle_scope(isolate());
    v8::Local<v8::Context> context = MainContext();

    script_context_ = CreateScriptContext(context, mutable_extension.get(),
                                          Feature::BLESSED_EXTENSION_CONTEXT);
    script_context_->set_url(extension_->url());
    bindings_system()->UpdateBindingsForContext(script_context_);
  }
  void TearDown() override {
    script_context_ = nullptr;
    extension_ = nullptr;
    messaging_service_.reset();
    NativeExtensionBindingsSystemUnittest::TearDown();
  }
  bool UseStrictIPCMessageSender() override { return true; }

  virtual scoped_refptr<Extension> BuildExtension() {
    return ExtensionBuilder("foo").Build();
  }

  NativeRendererMessagingService* messaging_service() {
    return messaging_service_.get();
  }
  ScriptContext* script_context() { return script_context_; }
  const Extension* extension() { return extension_.get(); }

 private:
  std::unique_ptr<NativeRendererMessagingService> messaging_service_;

  ScriptContext* script_context_ = nullptr;
  scoped_refptr<const Extension> extension_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionHooksDelegateTest);
};

// Test chrome.extension messaging methods. Many of these are just aliased to
// chrome.runtime counterparts, but some others (like sendRequest) are
// implemented as hooks.
TEST_F(ExtensionHooksDelegateTest, MessagingSanityChecks) {
  v8::HandleScope handle_scope(isolate());

  MessageTarget self_target = MessageTarget::ForExtension(extension()->id());
  SendMessageTester tester(ipc_message_sender(), script_context(), 0,
                           "extension");

  const bool kExpectIncludeTlsChannelId = false;
  tester.TestConnect("", "", self_target, kExpectIncludeTlsChannelId);

  constexpr char kStandardMessage[] = R"({"data":"hello"})";
  tester.TestSendMessage("{data: 'hello'}", kStandardMessage, self_target,
                         false, SendMessageTester::CLOSED);
  tester.TestSendMessage("{data: 'hello'}, function() {}", kStandardMessage,
                         self_target, false, SendMessageTester::OPEN);

  tester.TestSendRequest("{data: 'hello'}", kStandardMessage, self_target,
                         SendMessageTester::CLOSED);
  tester.TestSendRequest("{data: 'hello'}, function() {}", kStandardMessage,
                         self_target, SendMessageTester::OPEN);

  // Ambiguous argument case ('hi' could be an id or a message); we massage it
  // into being the message because that's a required argument.
  tester.TestSendRequest("'hi', function() {}", "\"hi\"", self_target,
                         SendMessageTester::OPEN);
}

TEST_F(ExtensionHooksDelegateTest, SendRequestDisabled) {
  // Construct an extension for which sendRequest is disabled (unpacked
  // extension with an event page).
  // TODO(devlin): Add a SetBackgroundPage() to ExtensionBuilder?
  scoped_refptr<Extension> extension =
      ExtensionBuilder("foo")
          .MergeManifest(
              DictionaryBuilder()
                  .Set("background", DictionaryBuilder()
                                         .SetBoolean("persistent", false)
                                         .Set("page", "page.html")
                                         .Build())
                  .Build())
          .SetLocation(Manifest::UNPACKED)
          .Build();
  RegisterExtension(extension);

  v8::HandleScope handle_scope(isolate());
  v8::Local<v8::Context> context = AddContext();
  ScriptContext* script_context = CreateScriptContext(
      context, extension.get(), Feature::BLESSED_EXTENSION_CONTEXT);
  script_context->set_url(extension->url());
  bindings_system()->UpdateBindingsForContext(script_context);
  ASSERT_TRUE(messaging_util::IsSendRequestDisabled(script_context));

  enum AccessBehavior {
    THROWS,
    DOESNT_THROW,
  };

  auto check_access = [context](const char* object, AccessBehavior behavior) {
    SCOPED_TRACE(object);
    constexpr char kExpectedError[] =
        "Uncaught Error: extension.sendRequest, extension.onRequest, and "
        "extension.onRequestExternal are deprecated. Please use "
        "runtime.sendMessage, runtime.onMessage, and runtime.onMessageExternal "
        "instead.";
    v8::Local<v8::Function> function = FunctionFromString(
        context, base::StringPrintf("(function() {%s;})", object));
    if (behavior == THROWS)
      RunFunctionAndExpectError(function, context, 0, nullptr, kExpectedError);
    else
      RunFunction(function, context, 0, nullptr);
  };

  check_access("chrome.extension.sendRequest", THROWS);
  check_access("chrome.extension.onRequest", THROWS);
  check_access("chrome.extension.onRequestExternal", THROWS);
  check_access("chrome.extension.sendMessage", DOESNT_THROW);
  check_access("chrome.extension.onMessage", DOESNT_THROW);
  check_access("chrome.extension.onMessageExternal", DOESNT_THROW);
}

}  // namespace extensions
