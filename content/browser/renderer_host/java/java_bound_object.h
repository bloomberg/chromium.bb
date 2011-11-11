// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BOUND_OBJECT_H_
#define CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BOUND_OBJECT_H_

#include <jni.h>
#include <map>
#include <string>

#include "base/android/scoped_java_ref.h"
#include "base/memory/linked_ptr.h"
#include "content/browser/renderer_host/java/java_method.h"
#include "third_party/npapi/bindings/npruntime.h"

// Wrapper around a Java object.
//
// Represents a Java object for use in the Java bridge. Holds a global ref to
// the Java object and provides the ability to invoke methods on it.
// Interrogation of the Java object for its methods is done lazily. This class
// is not generally threadsafe. However, it does allow for instances to be
// created and destroyed on different threads.
class JavaBoundObject {
 public:
  // Takes a Java object and creates a JavaBoundObject around it. Returns an
  // NPObject with a ref count of one which owns the JavaBoundObject.
  static NPObject* Create(const base::android::JavaRef<jobject>& object);

  virtual ~JavaBoundObject();

  // Gets the underlying JavaObject from a JavaBoundObject wrapped as an
  // NPObject.
  static jobject GetJavaObject(NPObject* object);

  // Methods to implement the NPObject callbacks.
  bool HasMethod(const std::string& name) const;
  bool Invoke(const std::string& name, const NPVariant* args, size_t arg_count,
              NPVariant* result);

 private:
  explicit JavaBoundObject(const base::android::JavaRef<jobject>& object);

  void EnsureMethodsAreSetUp() const;

  // Global ref to the underlying Java object. We use a naked jobject, rather
  // than a ScopedJavaGlobalRef, as the global ref will be added and dropped on
  // different threads.
  jobject java_object_;

  // Map of public methods, from method name to Method instance. Multiple
  // entries will be present for overloaded methods. Note that we can't use
  // scoped_ptr in STL containers as we can't copy it.
  typedef std::multimap<std::string, linked_ptr<JavaMethod> > JavaMethodMap;
  mutable JavaMethodMap methods_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(JavaBoundObject);
};

#endif  // CONTENT_BROWSER_RENDERER_HOST_JAVA_JAVA_BOUND_OBJECT_H_
