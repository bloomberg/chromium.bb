// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EVENT_BINDINGS_H_
#define EXTENSIONS_RENDERER_EVENT_BINDINGS_H_

#include "extensions/renderer/object_backed_native_handler.h"
#include "v8/include/v8.h"

namespace base {
class DictionaryValue;
}

namespace extensions {
class Dispatcher;
class EventFilter;
class EventFilteringInfo;
class EventMatcher;

// This class deals with the javascript bindings related to Event objects.
class EventBindings : public ObjectBackedNativeHandler {
 public:
  EventBindings(Dispatcher* dispatcher, ScriptContext* context);
  virtual ~EventBindings();

 private:
  // Attach an event name to an object.
  // |event_name| The name of the event to attach.
  void AttachEvent(const v8::FunctionCallbackInfo<v8::Value>& args);

  // Detach an event name from an object.
  // |event_name| The name of the event to stop listening to.
  // |is_manual| True if this detach was done by the user via removeListener()
  // as opposed to automatically during shutdown, in which case we should inform
  // the browser we are no longer interested in that event.
  void DetachEvent(const v8::FunctionCallbackInfo<v8::Value>& args);

  // MatcherID AttachFilteredEvent(string event_name, object filter)
  // |event_name| Name of the event to attach.
  // |filter| Which instances of the named event are we interested in.
  // returns the id assigned to the listener, which will be returned from calls
  // to MatchAgainstEventFilter where this listener matches.
  void AttachFilteredEvent(const v8::FunctionCallbackInfo<v8::Value>& args);

  // void DetachFilteredEvent(int id, bool manual)
  // id     - Id of the event to detach.
  // manual - false if this is part of the extension unload process where all
  //          listeners are automatically detached.
  void DetachFilteredEvent(const v8::FunctionCallbackInfo<v8::Value>& args);

  void MatchAgainstEventFilter(const v8::FunctionCallbackInfo<v8::Value>& args);

  Dispatcher* dispatcher_;
  scoped_ptr<EventMatcher> ParseEventMatcher(
      base::DictionaryValue* filter_dict);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EVENT_BINDINGS_H_
