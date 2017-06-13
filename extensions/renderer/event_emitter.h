// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EVENT_EMITTER_H_
#define EXTENSIONS_RENDERER_EVENT_EMITTER_H_

#include <vector>

#include "extensions/renderer/api_binding_types.h"
#include "gin/wrappable.h"
#include "v8/include/v8.h"

namespace gin {
class Arguments;
}

namespace extensions {
class APIEventListeners;
class EventFilteringInfo;

// A gin::Wrappable Event object. One is expected to be created per event, per
// context. Note: this object *does not* clear any events, so it must be
// destroyed with the context to avoid leaking.
class EventEmitter final : public gin::Wrappable<EventEmitter> {
 public:
  EventEmitter(bool supports_filters,
               std::unique_ptr<APIEventListeners> listeners,
               const binding::RunJSFunction& run_js,
               const binding::RunJSFunctionSync& run_js_sync);
  ~EventEmitter() override;

  static gin::WrapperInfo kWrapperInfo;

  // gin::Wrappable:
  gin::ObjectTemplateBuilder GetObjectTemplateBuilder(
      v8::Isolate* isolate) final;

  void Fire(v8::Local<v8::Context> context,
            std::vector<v8::Local<v8::Value>>* args,
            const EventFilteringInfo* filter);

  // Removes all listeners and marks this object as invalid so that no more
  // are added.
  void Invalidate(v8::Local<v8::Context> context);

  // TODO(devlin): Consider making this a test-only method and exposing
  // HasListeners() instead.
  size_t GetNumListeners() const;

 private:
  // Bound methods for the Event JS object.
  void AddListener(gin::Arguments* arguments);
  void RemoveListener(gin::Arguments* arguments);
  bool HasListener(v8::Local<v8::Function> function);
  bool HasListeners();
  void Dispatch(gin::Arguments* arguments);

  // Notifies the listeners of an event with the given |args|. If |run_sync| is
  // true, runs JS synchronously and populates |out_values| with the results of
  // the listeners.
  void DispatchImpl(v8::Local<v8::Context> context,
                    std::vector<v8::Local<v8::Value>>* args,
                    const EventFilteringInfo* filter,
                    bool run_sync,
                    std::vector<v8::Global<v8::Value>>* out_values);

  // Whether or not this object is still valid; false upon context release.
  // When invalid, no listeners can be added or removed.
  bool valid_ = true;

  // Whether the event supports filters.
  bool supports_filters_ = false;

  std::unique_ptr<APIEventListeners> listeners_;

  binding::RunJSFunction run_js_;
  binding::RunJSFunctionSync run_js_sync_;

  DISALLOW_COPY_AND_ASSIGN(EventEmitter);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EVENT_EMITTER_H_
