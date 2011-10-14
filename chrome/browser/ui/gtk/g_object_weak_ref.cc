// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/g_object_weak_ref.h"

#include "base/logging.h"

class GObjectWeakRef::GObjectWeakRefDelegate
    : public base::RefCounted<GObjectWeakRefDelegate> {
 public:
  explicit GObjectWeakRefDelegate(GObject* object);

  GObject* get() const { return object_; }

 private:
  friend class base::RefCounted<GObjectWeakRefDelegate>;

  ~GObjectWeakRefDelegate();

  static void OnFinalizedThunk(gpointer data, GObject* object);
  void OnFinalized(GObject* object);

  GObject* object_;

  DISALLOW_COPY_AND_ASSIGN(GObjectWeakRefDelegate);
};

GObjectWeakRef::GObjectWeakRefDelegate::GObjectWeakRefDelegate(GObject* object)
    : object_(object) {
  if (object)
    g_object_weak_ref(object, OnFinalizedThunk, this);
}

GObjectWeakRef::GObjectWeakRefDelegate::~GObjectWeakRefDelegate() {
  if (object_)
    g_object_weak_unref(object_, OnFinalizedThunk, this);
}

// static
void GObjectWeakRef::GObjectWeakRefDelegate::OnFinalizedThunk(gpointer data,
                                                              GObject* object) {
  static_cast<GObjectWeakRefDelegate*>(data)->OnFinalized(object);
}

void GObjectWeakRef::GObjectWeakRefDelegate::OnFinalized(GObject* object) {
  DCHECK_EQ(object_, object);
  object_ = NULL;
}

GObjectWeakRef::GObjectWeakRef(GObject* object) {
  reset(object);
}

GObjectWeakRef::GObjectWeakRef(GtkWidget* widget) {
  reset(G_OBJECT(widget));
}

GObjectWeakRef::GObjectWeakRef(const GObjectWeakRef& ref) : ref_(ref.ref_) {}

GObjectWeakRef::~GObjectWeakRef() {}

GObjectWeakRef& GObjectWeakRef::operator=(GObject* object) {
  reset(object);
  return *this;
}

GObjectWeakRef& GObjectWeakRef::operator=(GtkWidget* widget) {
  reset(G_OBJECT(widget));
  return *this;
}

GObjectWeakRef& GObjectWeakRef::operator=(const GObjectWeakRef& ref) {
  ref_ = ref.ref_;
  return *this;
}

GObject* GObjectWeakRef::get() const {
  return ref_->get();
}

void GObjectWeakRef::reset(GObject* object) {
  ref_ = new GObjectWeakRefDelegate(object);
}
