// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/module_system_test.h"

#include "grit/renderer_resources.h"

namespace extensions {
namespace {

class EventUnittest : public ModuleSystemTest {
  virtual void SetUp() OVERRIDE {
    ModuleSystemTest::SetUp();
    module_system_->RunString("chrome = {};", "setup-chrome");

    RegisterModule("event", IDR_EVENT_BINDINGS_JS);
    RegisterModule("schemaUtils", IDR_SCHEMA_UTILS_JS);
    RegisterModule("utils", IDR_UTILS_JS);

    // Mock out the native handler for event_bindings. These mocks will fail if
    // any invariants maintained by the real event_bindings are broken.
    OverrideNativeHandler("event_bindings",
        "var assert = requireNative('assert');"
        "var attachedListeners = exports.attachedListeners = {};"
        "var attachedFilteredListeners = "
        "    exports.attachedFilteredListeners = {};"
        "var nextId = 0;"
        "var idToName = {};"

        "exports.AttachEvent = function(eventName) {"
        "  assert.AssertFalse(!!attachedListeners[eventName]);"
        "  attachedListeners[eventName] = 1;"
        "};"

        "exports.DetachEvent = function(eventName) {"
        "  assert.AssertTrue(!!attachedListeners[eventName]);"
        "  delete attachedListeners[eventName];"
        "};"

        "exports.IsEventAttached = function(eventName) {"
        "  return !!attachedListeners[eventName];"
        "};"

        "exports.AttachFilteredEvent = function(name, filters) {"
        "  var id = nextId++;"
        "  idToName[id] = name;"
        "  attachedFilteredListeners[name] ="
        "    attachedFilteredListeners[name] || [];"
        "  attachedFilteredListeners[name][id] = filters;"
        "  return id;"
        "};"

        "exports.DetachFilteredEvent = function(id, manual) {"
        "  var i = attachedFilteredListeners[idToName[id]].indexOf(id);"
        "  attachedFilteredListeners[idToName[id]].splice(i, 1);"
        "};"

        "exports.HasFilteredListener = function(name) {"
        "  return attachedFilteredListeners[name].length;"
        "};");
    OverrideNativeHandler("chrome_hidden",
        "var chromeHidden = {};"
        "exports.GetChromeHidden = function() { return chromeHidden; };");
    OverrideNativeHandler("chrome",
        "var chrome = {};"
        "exports.GetChrome = function() { return chrome; };");
    OverrideNativeHandler("sendRequest",
        "exports.sendRequest = function() {};");
    OverrideNativeHandler("apiDefinitions",
        "exports.GetExtensionAPIDefinition = function() {};");
    OverrideNativeHandler("logging",
        "exports.DCHECK = function() {};");
  }
};

TEST_F(EventUnittest, TestNothing) {
  ExpectNoAssertionsMade();
}

TEST_F(EventUnittest, AddRemoveTwoListeners) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "var assert = requireNative('assert');"
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var eventBindings = requireNative('event_bindings');"
      "var myEvent = new Event('named-event');"
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

TEST_F(EventUnittest, OnUnloadDetachesAllListeners) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "var assert = requireNative('assert');"
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var eventBindings = requireNative('event_bindings');"
      "var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();"
      "var myEvent = new Event('named-event');"
      "var cb1 = function() {};"
      "var cb2 = function() {};"
      "myEvent.addListener(cb1);"
      "myEvent.addListener(cb2);"
      "chromeHidden.dispatchOnUnload();"
      "assert.AssertFalse(!!eventBindings.attachedListeners['named-event']);");
  module_system_->Require("test");
}

TEST_F(EventUnittest, OnUnloadDetachesAllListenersEvenDupes) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "var assert = requireNative('assert');"
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var eventBindings = requireNative('event_bindings');"
      "var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();"
      "var myEvent = new Event('named-event');"
      "var cb1 = function() {};"
      "myEvent.addListener(cb1);"
      "myEvent.addListener(cb1);"
      "chromeHidden.dispatchOnUnload();"
      "assert.AssertFalse(!!eventBindings.attachedListeners['named-event']);");
  module_system_->Require("test");
}

TEST_F(EventUnittest, EventsThatSupportRulesMustHaveAName) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var eventOpts = {supportsRules: true};"
      "var assert = requireNative('assert');"
      "var caught = false;"
      "try {"
      "  var myEvent = new Event(undefined, undefined, eventOpts);"
      "} catch (e) {"
      "  caught = true;"
      "}"
      "assert.AssertTrue(caught);");
  module_system_->Require("test");
}

TEST_F(EventUnittest, NamedEventDispatch) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var chromeHidden = requireNative('chrome_hidden').GetChromeHidden();"
      "var assert = requireNative('assert');"
      "var e = new Event('myevent');"
      "var called = false;"
      "e.addListener(function() { called = true; });"
      "chromeHidden.Event.dispatchEvent('myevent', []);"
      "assert.AssertTrue(called);");
  module_system_->Require("test");
}

TEST_F(EventUnittest, AddListenerWithFiltersThrowsErrorByDefault) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var assert = requireNative('assert');"
      "var e = new Event('myevent');"
      "var filter = [{"
      "  url: {hostSuffix: 'google.com'},"
      "}];"
      "var caught = false;"
      "try {"
      "  e.addListener(function() {}, filter);"
      "} catch (e) {"
      "  caught = true;"
      "}"
      "assert.AssertTrue(caught);");
  module_system_->Require("test");
}

TEST_F(EventUnittest, FilteredEventsAttachment) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var assert = requireNative('assert');"
      "var bindings = requireNative('event_bindings');"
      "var eventOpts = {supportsListeners: true, supportsFilters: true};"
      "var e = new Event('myevent', undefined, eventOpts);"
      "var cb = function() {};"
      "var filters = {url: [{hostSuffix: 'google.com'}]};"
      "e.addListener(cb, filters);"
      "assert.AssertTrue(bindings.HasFilteredListener('myevent'));"
      "e.removeListener(cb);"
      "assert.AssertFalse(bindings.HasFilteredListener('myevent'));");
  module_system_->Require("test");
}

TEST_F(EventUnittest, DetachFilteredEvent) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var assert = requireNative('assert');"
      "var bindings = requireNative('event_bindings');"
      "var eventOpts = {supportsListeners: true, supportsFilters: true};"
      "var e = new Event('myevent', undefined, eventOpts);"
      "var cb1 = function() {};"
      "var cb2 = function() {};"
      "var filters = {url: [{hostSuffix: 'google.com'}]};"
      "e.addListener(cb1, filters);"
      "e.addListener(cb2, filters);"
      "e.detach_();"
      "assert.AssertFalse(bindings.HasFilteredListener('myevent'));");
  module_system_->Require("test");
}

TEST_F(EventUnittest, AttachAndRemoveSameFilteredEventListener) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var assert = requireNative('assert');"
      "var bindings = requireNative('event_bindings');"
      "var eventOpts = {supportsListeners: true, supportsFilters: true};"
      "var e = new Event('myevent', undefined, eventOpts);"
      "var cb = function() {};"
      "var filters = {url: [{hostSuffix: 'google.com'}]};"
      "e.addListener(cb, filters);"
      "e.addListener(cb, filters);"
      "assert.AssertTrue(bindings.HasFilteredListener('myevent'));"
      "e.removeListener(cb);"
      "assert.AssertTrue(bindings.HasFilteredListener('myevent'));"
      "e.removeListener(cb);"
      "assert.AssertFalse(bindings.HasFilteredListener('myevent'));");
  module_system_->Require("test");
}

TEST_F(EventUnittest, AddingFilterWithUrlFieldNotAListThrowsException) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var assert = requireNative('assert');"
      "var eventOpts = {supportsListeners: true, supportsFilters: true};"
      "var e = new Event('myevent', undefined, eventOpts);"
      "var cb = function() {};"
      "var filters = {url: {hostSuffix: 'google.com'}};"
      "var caught = false;"
      "try {"
      "  e.addListener(cb, filters);"
      "} catch (e) {"
      "  caught = true;"
      "}"
      "assert.AssertTrue(caught);");
  module_system_->Require("test");
}

TEST_F(EventUnittest, MaxListeners) {
  ModuleSystem::NativesEnabledScope natives_enabled_scope(module_system_.get());
  RegisterModule("test",
      "require('event');"
      "var Event = requireNative('chrome').GetChrome().Event;"
      "var assert = requireNative('assert');"
      "var eventOpts = {supportsListeners: true, maxListeners: 1};"
      "var e = new Event('myevent', undefined, eventOpts);"
      "var cb = function() {};"
      "var caught = false;"
      "try {"
      "  e.addListener(cb);"
      "} catch (e) {"
      "  caught = true;"
      "}"
      "assert.AssertTrue(!caught);"
      "try {"
      "  e.addListener(cb);"
      "} catch (e) {"
      "  caught = true;"
      "}"
      "assert.AssertTrue(caught);");
  module_system_->Require("test");
}

}  // namespace
}  // namespace extensions
