// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GESTURES_PROP_REGISTRY_H__
#define GESTURES_PROP_REGISTRY_H__

#include <set>
#include <string>

#include <base/string_util.h>

#include "gestures/include/gestures.h"
#include "gestures/include/logging.h"

namespace gestures {

class Property;

class PropRegistry {
 public:
  PropRegistry() : prop_provider_(NULL) {}

  void Register(Property* prop);
  void Unregister(Property* prop);

  void SetPropProvider(GesturesPropProvider* prop_provider, void* data);
  GesturesPropProvider* PropProvider() const { return prop_provider_; }
  void* PropProviderData() const { return prop_provider_data_; }

 private:
  GesturesPropProvider* prop_provider_;
  void* prop_provider_data_;
  std::set<Property*> props_;
};

class PropertyDelegate;

class Property {
 public:
  Property(PropRegistry* parent, const char* name)
      : parent_(parent), delegate_(NULL), name_(name) {}
  Property(PropRegistry* parent, const char* name, PropertyDelegate* delegate)
      : parent_(parent), delegate_(delegate), name_(name) {}

  virtual ~Property() {
    if (parent_)
      parent_->Unregister(this);
  }

  void CreateProp();
  virtual void CreatePropImpl() = 0;
  void DestroyProp();

  const char* name() { return name_; }
  virtual std::string Value() = 0;

  static GesturesPropBool StaticHandleGesturesPropWillRead(void* data) {
    GesturesPropBool ret =
        reinterpret_cast<Property*>(data)->HandleGesturesPropWillRead();
    return ret;
  }
  // TODO(adlr): pass on will-read notifications
  virtual GesturesPropBool HandleGesturesPropWillRead() { return 0; }
  static void StaticHandleGesturesPropWritten(void* data) {
    reinterpret_cast<Property*>(data)->HandleGesturesPropWritten();
  }
  virtual void HandleGesturesPropWritten() = 0;

 protected:
  GesturesProp* gprop_;
  PropRegistry* parent_;
  PropertyDelegate* delegate_;

 private:
  const char* name_;
};

class BoolProperty : public Property {
 public:
  BoolProperty(PropRegistry* reg, const char* name, GesturesPropBool val)
      : Property(reg, name), val_(val) {
    if (parent_)
      parent_->Register(this);
  }
  BoolProperty(PropRegistry* reg, const char* name, GesturesPropBool val,
               PropertyDelegate* delegate)
      : Property(reg, name, delegate), val_(val) {
    if (parent_)
      parent_->Register(this);
  }
  virtual void CreatePropImpl();
  virtual std::string Value();
  virtual void HandleGesturesPropWritten();

  GesturesPropBool val_;
};

class DoubleProperty : public Property {
 public:
  DoubleProperty(PropRegistry* reg, const char* name, double val)
      : Property(reg, name), val_(val) {
    if (parent_)
      parent_->Register(this);
  }
  DoubleProperty(PropRegistry* reg, const char* name, double val,
                 PropertyDelegate* delegate)
      : Property(reg, name, delegate), val_(val) {
    if (parent_)
      parent_->Register(this);
  }
  virtual void CreatePropImpl();
  virtual std::string Value();
  virtual void HandleGesturesPropWritten();

  double val_;
};

class IntProperty : public Property {
 public:
  IntProperty(PropRegistry* reg, const char* name, int val)
      : Property(reg, name), val_(val) {
    if (parent_)
      parent_->Register(this);
  }
  IntProperty(PropRegistry* reg, const char* name, int val,
              PropertyDelegate* delegate)
      : Property(reg, name, delegate), val_(val) {
    if (parent_)
      parent_->Register(this);
  }
  virtual void CreatePropImpl();
  virtual std::string Value();
  virtual void HandleGesturesPropWritten();

  int val_;
};

class ShortProperty : public Property {
 public:
  ShortProperty(PropRegistry* reg, const char* name, short val)
      : Property(reg, name), val_(val) {
    if (parent_)
      parent_->Register(this);
  }
  ShortProperty(PropRegistry* reg, const char* name, short val,
                PropertyDelegate* delegate)
      : Property(reg, name, delegate), val_(val) {
    if (parent_)
      parent_->Register(this);
  }
  virtual void CreatePropImpl();
  virtual std::string Value();
  virtual void HandleGesturesPropWritten();

  short val_;
};

class StringProperty : public Property {
 public:
  StringProperty(PropRegistry* reg, const char* name, const char* val)
      : Property(reg, name), val_(val) {
    if (parent_)
      parent_->Register(this);
  }
  StringProperty(PropRegistry* reg, const char* name, const char* val,
                 PropertyDelegate* delegate)
      : Property(reg, name, delegate), val_(val) {
    if (parent_)
      parent_->Register(this);
  }
  virtual void CreatePropImpl();
  virtual std::string Value();
  virtual void HandleGesturesPropWritten();

  const char* val_;
};

class PropertyDelegate {
 public:
  virtual void BoolWasWritten(BoolProperty* prop) {};
  virtual void DoubleWasWritten(DoubleProperty* prop) {};
  virtual void IntWasWritten(IntProperty* prop) {};
  virtual void ShortWasWritten(ShortProperty* prop) {};
  virtual void StringWasWritten(StringProperty* prop) {};
};

}  // namespace gestures

#endif  // GESTURES_PROP_REGISTRY_H__
