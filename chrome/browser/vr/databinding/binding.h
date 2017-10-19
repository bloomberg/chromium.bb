// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VR_DATABINDING_BINDING_H_
#define CHROME_BROWSER_VR_DATABINDING_BINDING_H_

#include "base/bind.h"
#include "base/macros.h"
#include "base/optional.h"
#include "chrome/browser/vr/databinding/binding_base.h"

namespace vr {

// This class represents a one-way binding that propagates a change from a
// source/model property to a sink/view. This is inappropriate for use in the
// case of, say, an editor view like a text field where changes must also be
// propagated back to the model.
//
// IMPORTANT: it is assumed that a Binding instance will outlive the model to
// which it is bound. This class in not appropriate for use with models that
// come and go in the lifetime of the application.
template <typename T>
class Binding : public BindingBase {
 public:
  Binding(const base::Callback<T()>& getter,
          const base::Callback<void(const T&)>& setter)
      : getter_(getter), setter_(setter) {}
  ~Binding() override {}

  // This function will check if the getter is producing a different value than
  // when it was last polled. If so, it will pass that value to the provided
  // setter. NB: this assumes that T is copyable.
  bool Update() override {
    T current_value = getter_.Run();
    if (last_value_ && current_value == last_value_.value())
      return false;
    last_value_ = current_value;
    setter_.Run(current_value);
    return true;
  }

 private:
  base::Callback<T()> getter_;
  base::Callback<void(const T&)> setter_;
  base::Optional<T> last_value_;

  DISALLOW_COPY_AND_ASSIGN(Binding);
};

// These macros are sugar for constructing a simple binding. It is meant to make
// setting up bindings a little less painful, but it is not meant to handle all
// cases. If you need to do something more complex (eg, convert type T before
// it is propagated), you should use the constructor directly.
//
// For example:
//
// struct MyModel { int source; };
// struct MyView {
//   int sink;
//   int awesomeness;
//   void SetAwesomeness(int new_awesomeness) {
//     awesomeness = new_awesomeness;
//   }
// };
//
// MyModel m;
// m.source = 20;
//
// MyView v;
// v.sink = 10;
// v.awesomeness = 30;
//
// auto binding = VR_BIND(int, MyModel, &m, source, MyView, &v, sink = value);
//
// Or, equivalently:
//
// auto binding = VR_BIND_FIELD(int, MyModel, &m, source, MyView, &v, sink);
//
// If your view has a setter, you may find VR_BIND_FUNC handy:
//
// auto binding =
//     VR_BIND_FUNC(int, MyModel, &m, source, MyView, &v, SetAwesomeness);
//
#define VR_BIND(T, M, m, Get, V, v, Set)                                    \
  base::MakeUnique<Binding<T>>(                                             \
      base::Bind([](M* model) { return model->Get; }, base::Unretained(m)), \
      base::Bind([](V* view, const T& value) { view->Set; },                \
                 base::Unretained(v)))

#define VR_BIND_FUNC(T, M, m, Get, V, v, f) \
  VR_BIND(T, M, m, Get, V, v, f(value))

#define VR_BIND_FIELD(T, M, m, Get, V, v, f) \
  VR_BIND(T, M, m, Get, V, v, f = value)

}  // namespace vr

#endif  // CHROME_BROWSER_VR_DATABINDING_BINDING_H_
