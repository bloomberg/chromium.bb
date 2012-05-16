// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/module_system_test.h"

#include "grit/renderer_resources.h"

namespace {

class EventUnittest : public ModuleSystemTest {
  virtual void SetUp() OVERRIDE {
    ModuleSystemTest::SetUp();
    module_system_->RunString("chrome = {};", "setup-chrome");

    RegisterModule("event", IDR_EVENT_BINDINGS_JS);

    // Mock out the native handler for event_bindings. These mocks will fail if
    // any invariants maintained by the real event_bindings are broken.
    OverrideNativeHandler("event_bindings",
        "var assert = requireNative('assert');"
        "var attachedListeners = exports.attachedListeners = {};"
        "exports.AttachEvent = function(eventName) {"
        "  assert.AssertFalse(!!attachedListeners[eventName]);"
        "  attachedListeners[eventName] = 1;"
        "};"
        "exports.DetachEvent = function(eventName) {"
        "  assert.AssertTrue(!!attachedListeners[eventName]);"
        "  delete attachedListeners[eventName];"
        "};");
    OverrideNativeHandler("chrome_hidden",
        "var chromeHidden = {};"
        "exports.GetChromeHidden = function() { return chromeHidden; };");
  }
};

TEST_F(EventUnittest, TestNothing) {
  ExpectNoAssertionsMade();
}

TEST_F(EventUnittest, AddRemoveTwoListeners) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "var assert = requireNative('assert');"
      "var event = require('event');"
      "var eventBindings = requireNative('event_bindings');"
      "var myEvent = new event.Event('named-event');"
      "var cb1 = function() {};"
      "var cb2 = function() {};"
      "myEvent.addListener(cb1);"
      "myEvent.addListener(cb2);"
      "myEvent.removeListener(cb1);"
      "assert.AssertTrue(!!eventBindings.attachedListeners['named-event']);"
      "myEvent.removeListener(cb2);"
      "assert.AssertFalse(!!eventBindings.attachedListeners['named-event']);");
  module_system_->Require("test");
}

TEST_F(EventUnittest, EventsThatSupportRulesMustHaveAName) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "var event = require('event');"
      "var eventOpts = {supportsRules: true};"
      "var assert = requireNative('assert');"
      "var caught = false;"
      "try {"
      "  var myEvent = new event.Event(undefined, undefined, eventOpts);"
      "} catch (e) {"
      "  caught = true;"
      "}"
      "assert.AssertTrue(caught);");
  module_system_->Require("test");
}

TEST_F(EventUnittest, NamedEventDispatch) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "var event = require('event');"
      "var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();"
      "var assert = requireNative('assert');"
      "var e = new event.Event('myevent');"
      "var called = false;"
      "e.addListener(function() { called = true; });"
      "chromeHidden.Event.dispatch('myevent', []);"
      "assert.AssertTrue(called);");
  module_system_->Require("test");
}

}  // namespace
