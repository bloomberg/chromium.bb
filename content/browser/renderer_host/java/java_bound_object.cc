// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_bound_object.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/memory/singleton.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "content/browser/renderer_host/java/java_type.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"

using base::StringPrintf;
using base::android::AttachCurrentThread;
using base::android::ConvertUTF8ToJavaString;
using base::android::JavaRef;
using base::android::MethodID;
using base::android::ScopedJavaGlobalRef;
using base::android::ScopedJavaLocalRef;
using WebKit::WebBindings;

// The conversion between JavaScript and Java types is based on the Live
// Connect 2 spec. See
// http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_CONVERSIONS.

// Note that in some cases, we differ from from the spec in order to maintain
// existing behavior. These areas are marked LIVECONNECT_COMPLIANCE. We may
// revisit this decision in the future.

namespace {

// Our special NPObject type.  We extend an NPObject with a pointer to a
// JavaBoundObject.  We also add static methods for each of the NPObject
// callbacks, which are registered by our NPClass. These methods simply
// delegate to the private implementation methods of JavaBoundObject.
struct JavaNPObject : public NPObject {
  JavaBoundObject* bound_object;

  static const NPClass kNPClass;

  static NPObject* Allocate(NPP npp, NPClass* np_class);
  static void Deallocate(NPObject* np_object);
  static bool HasMethod(NPObject* np_object, NPIdentifier np_identifier);
  static bool Invoke(NPObject* np_object, NPIdentifier np_identifier,
                     const NPVariant *args, uint32_t arg_count,
                     NPVariant *result);
  static bool HasProperty(NPObject* np_object, NPIdentifier np_identifier);
  static bool GetProperty(NPObject* np_object, NPIdentifier np_identifier,
                          NPVariant *result);
};

const NPClass JavaNPObject::kNPClass = {
  NP_CLASS_STRUCT_VERSION,
  JavaNPObject::Allocate,
  JavaNPObject::Deallocate,
  NULL,  // NPInvalidate
  JavaNPObject::HasMethod,
  JavaNPObject::Invoke,
  NULL,  // NPInvokeDefault
  JavaNPObject::HasProperty,
  JavaNPObject::GetProperty,
  NULL,  // NPSetProperty,
  NULL,  // NPRemoveProperty
};

NPObject* JavaNPObject::Allocate(NPP npp, NPClass* np_class) {
  JavaNPObject* obj = new JavaNPObject();
  return obj;
}

void JavaNPObject::Deallocate(NPObject* np_object) {
  JavaNPObject* obj = reinterpret_cast<JavaNPObject*>(np_object);
  delete obj->bound_object;
  delete obj;
}

bool JavaNPObject::HasMethod(NPObject* np_object, NPIdentifier np_identifier) {
  std::string name(WebBindings::utf8FromIdentifier(np_identifier));
  JavaNPObject* obj = reinterpret_cast<JavaNPObject*>(np_object);
  return obj->bound_object->HasMethod(name);
}

bool JavaNPObject::Invoke(NPObject* np_object, NPIdentifier np_identifier,
                          const NPVariant* args, uint32_t arg_count,
                          NPVariant* result) {
  std::string name(WebBindings::utf8FromIdentifier(np_identifier));
  JavaNPObject* obj = reinterpret_cast<JavaNPObject*>(np_object);
  return obj->bound_object->Invoke(name, args, arg_count, result);
}

bool JavaNPObject::HasProperty(NPObject* np_object,
                               NPIdentifier np_identifier) {
  // LIVECONNECT_COMPLIANCE: Return false to indicate that the property is not
  // present. We should support this correctly.
  return false;
}

bool JavaNPObject::GetProperty(NPObject* np_object,
                               NPIdentifier np_identifier,
                               NPVariant* result) {
  // LIVECONNECT_COMPLIANCE: Return false to indicate that the property is
  // undefined. We should support this correctly.
  return false;
}

// Calls a Java method through JNI and returns the result as an NPVariant. Note
// that this method does not do any type coercion. The Java return value is
// simply converted to the corresponding NPAPI type.
NPVariant CallJNIMethod(jobject object, const JavaType& return_type,
                        jmethodID id, jvalue* parameters) {
  JNIEnv* env = AttachCurrentThread();
  NPVariant result;
  switch (return_type.type) {
    case JavaType::TypeBoolean:
      BOOLEAN_TO_NPVARIANT(env->CallBooleanMethodA(object, id, parameters),
                           result);
      break;
    case JavaType::TypeByte:
      INT32_TO_NPVARIANT(env->CallByteMethodA(object, id, parameters), result);
      break;
    case JavaType::TypeChar:
      INT32_TO_NPVARIANT(env->CallCharMethodA(object, id, parameters), result);
      break;
    case JavaType::TypeShort:
      INT32_TO_NPVARIANT(env->CallShortMethodA(object, id, parameters), result);
      break;
    case JavaType::TypeInt:
      INT32_TO_NPVARIANT(env->CallIntMethodA(object, id, parameters), result);
      break;
    case JavaType::TypeLong:
      DOUBLE_TO_NPVARIANT(env->CallLongMethodA(object, id, parameters), result);
      break;
    case JavaType::TypeFloat:
      DOUBLE_TO_NPVARIANT(env->CallFloatMethodA(object, id, parameters),
                          result);
      break;
    case JavaType::TypeDouble:
      DOUBLE_TO_NPVARIANT(env->CallDoubleMethodA(object, id, parameters),
                          result);
      break;
    case JavaType::TypeVoid:
      env->CallVoidMethodA(object, id, parameters);
      VOID_TO_NPVARIANT(result);
      break;
    case JavaType::TypeArray:
      // TODO(steveblock): Handle arrays
      VOID_TO_NPVARIANT(result);
      break;
    case JavaType::TypeString: {
      ScopedJavaLocalRef<jstring> java_string(env, static_cast<jstring>(
          env->CallObjectMethodA(object, id, parameters)));
      if (!java_string.obj()) {
        // LIVECONNECT_COMPLIANCE: Return undefined to maintain existing
        // behavior. We should return a null string.
        VOID_TO_NPVARIANT(result);
        break;
      }
      std::string str =
          base::android::ConvertJavaStringToUTF8(env, java_string.obj());
      // Take a copy and pass ownership to the variant. We must allocate using
      // NPN_MemAlloc, to match NPN_ReleaseVariant, which uses NPN_MemFree.
      size_t length = str.length();
      char* buffer = static_cast<char*>(NPN_MemAlloc(length));
      str.copy(buffer, length, 0);
      STRINGN_TO_NPVARIANT(buffer, length, result);
      break;
    }
    case JavaType::TypeObject: {
      ScopedJavaLocalRef<jobject> java_object(
          env,
          env->CallObjectMethodA(object, id, parameters));
      if (!java_object.obj()) {
        NULL_TO_NPVARIANT(result);
        break;
      }
      OBJECT_TO_NPVARIANT(JavaBoundObject::Create(java_object), result);
      break;
    }
  }
  return result;
}

jvalue CoerceJavaScriptNumberToJavaValue(const NPVariant& variant,
                                         const JavaType& target_type) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_NUMBER_VALUES.
  jvalue result;
  DCHECK(variant.type == NPVariantType_Int32 ||
         variant.type == NPVariantType_Double);
  bool is_double = variant.type == NPVariantType_Double;
  switch (target_type.type) {
    case JavaType::TypeByte:
      result.b = is_double ? static_cast<jbyte>(NPVARIANT_TO_DOUBLE(variant)) :
                             static_cast<jbyte>(NPVARIANT_TO_INT32(variant));
      break;
    case JavaType::TypeChar:
      // LIVECONNECT_COMPLIANCE: Convert double to 0 to maintain existing
      // behavior.
      result.c = is_double ? 0 :
                             static_cast<jchar>(NPVARIANT_TO_INT32(variant));
      break;
    case JavaType::TypeShort:
      result.s = is_double ? static_cast<jshort>(NPVARIANT_TO_DOUBLE(variant)) :
                             static_cast<jshort>(NPVARIANT_TO_INT32(variant));
      break;
    case JavaType::TypeInt:
      result.i = is_double ? static_cast<jint>(NPVARIANT_TO_DOUBLE(variant)) :
                             NPVARIANT_TO_INT32(variant);
      break;
    case JavaType::TypeLong:
      result.j = is_double ? static_cast<jlong>(NPVARIANT_TO_DOUBLE(variant)) :
                             NPVARIANT_TO_INT32(variant);
      break;
    case JavaType::TypeFloat:
      result.f = is_double ? static_cast<jfloat>(NPVARIANT_TO_DOUBLE(variant)) :
                             NPVARIANT_TO_INT32(variant);
      break;
    case JavaType::TypeDouble:
      result.d = is_double ? NPVARIANT_TO_DOUBLE(variant) :
                             NPVARIANT_TO_INT32(variant);
      break;
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Convert to null to maintain existing behavior.
      // We should handle object equivalents of primitive types.
      result.l = NULL;
      break;
    case JavaType::TypeString:
      result.l = ConvertUTF8ToJavaString(
          AttachCurrentThread(),
          is_double ?  StringPrintf("%.6lg", NPVARIANT_TO_DOUBLE(variant)) :
                       base::IntToString(NPVARIANT_TO_INT32(variant)));
      break;
    case JavaType::TypeBoolean:
      // LIVECONNECT_COMPLIANCE: Convert to false to maintain existing behavior.
      // We should convert to false for o or NaN, true otherwise.
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Convert to null to maintain existing behavior.
      // We should raise a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptBooleanToJavaValue(const NPVariant& variant,
                                          const JavaType& target_type) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_BOOLEAN_VALUES.
  DCHECK_EQ(NPVariantType_Bool, variant.type);
  bool boolean_value = NPVARIANT_TO_BOOLEAN(variant);
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeBoolean:
      result.z = boolean_value ? JNI_TRUE : JNI_FALSE;
      break;
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Convert to NULL to maintain existing behavior.
      // We should handle java.lang.Boolean and java.lang.Object.
      result.l = NULL;
      break;
    case JavaType::TypeString:
      result.l = ConvertUTF8ToJavaString(AttachCurrentThread(),
                                         boolean_value ? "true" : "false");
      break;
    case JavaType::TypeByte:
    case JavaType::TypeChar:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble: {
      // LIVECONNECT_COMPLIANCE: Convert to 0 to maintain existing behavior. We
      // should convert to 0 or 1.
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Convert to NULL to maintain existing behavior.
      // We should raise a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptStringToJavaValue(const NPVariant& variant,
                                         const JavaType& target_type) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_STRING_VALUES.
  DCHECK_EQ(NPVariantType_String, variant.type);
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeString:
      result.l = ConvertUTF8ToJavaString(
          AttachCurrentThread(),
          base::StringPiece(NPVARIANT_TO_STRING(variant).UTF8Characters,
                            NPVARIANT_TO_STRING(variant).UTF8Length));
      break;
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Convert to NULL to maintain existing behavior.
      // We should handle java.lang.Object.
      result.l = NULL;
      break;
    case JavaType::TypeByte:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble: {
      // LIVECONNECT_COMPLIANCE: Convert to 0 to maintain existing behavior. we
      // should use valueOf() method of corresponding object type.
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeChar:
      // LIVECONNECT_COMPLIANCE: Convert to 0 to maintain existing behavior. we
      // should use java.lang.Short.decode().
      result.c = 0;
      break;
    case JavaType::TypeBoolean:
      // LIVECONNECT_COMPLIANCE: Convert to false to maintain existing behavior.
      // We should convert the empty string to false, otherwise true.
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Convert to NULL to maintain existing behavior.
      // We should raise a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptObjectToJavaValue(const NPVariant& variant,
                                         const JavaType& target_type) {
  // We only handle Java objects. See
  // http://jdk6.java.net/plugin2/liveconnect/#JS_JAVA_OBJECTS.
  // TODO(steveblock): Handle arrays.
  // TODO(steveblock): Handle JavaScript objects.
  DCHECK_EQ(NPVariantType_Object, variant.type);

  // The only type of object we should encounter is a Java object, as
  // other objects should have been converted to NULL in the renderer.
  // See CreateNPVariantParam().
  // TODO(steveblock): This will have to change once we support arrays and
  // JavaScript objects.
  NPObject* object = NPVARIANT_TO_OBJECT(variant);
  DCHECK_EQ(&JavaNPObject::kNPClass, object->_class);

  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeObject:
      // LIVECONNECT_COMPLIANCE: Pass all Java objects to maintain existing
      // behavior. We should pass only Java objects which are
      // assignment-compatibile.
      result.l = AttachCurrentThread()->NewLocalRef(
          JavaBoundObject::GetJavaObject(object));
      break;
    case JavaType::TypeString:
      // LIVECONNECT_COMPLIANCE: Convert to "undefined" to maintain existing
      // behavior. We should call toString() on the Java object.
      result.l = ConvertUTF8ToJavaString(AttachCurrentThread(), "undefined");
      break;
    case JavaType::TypeByte:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble:
    case JavaType::TypeChar: {
      // LIVECONNECT_COMPLIANCE: Convert to 0 to maintain existing behavior. We
      // should raise a JavaScript exception.
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeBoolean:
      // LIVECONNECT_COMPLIANCE: Convert to false to maintain existing behavior.
      // We should raise a JavaScript exception.
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Convert to NULL to maintain existing behavior.
      // We should raise a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptNullOrUndefinedToJavaValue(const NPVariant& variant,
                                                  const JavaType& target_type) {
  // See http://jdk6.java.net/plugin2/liveconnect/#JS_NULL.
  DCHECK(variant.type == NPVariantType_Null ||
         variant.type == NPVariantType_Void);
  jvalue result;
  switch (target_type.type) {
    case JavaType::TypeObject:
      result.l = NULL;
      break;
    case JavaType::TypeString:
      if (variant.type == NPVariantType_Void) {
        // LIVECONNECT_COMPLIANCE: Convert undefined to "undefined" to maintain
        // existing behavior. We should convert undefined to NULL.
        result.l =  ConvertUTF8ToJavaString(AttachCurrentThread(), "undefined");
      } else {
        result.l = NULL;
      }
      break;
    case JavaType::TypeByte:
    case JavaType::TypeChar:
    case JavaType::TypeShort:
    case JavaType::TypeInt:
    case JavaType::TypeLong:
    case JavaType::TypeFloat:
    case JavaType::TypeDouble: {
      jvalue null_value = {0};
      result = null_value;
      break;
    }
    case JavaType::TypeBoolean:
      result.z = JNI_FALSE;
      break;
    case JavaType::TypeArray:
      // LIVECONNECT_COMPLIANCE: Convert to NULL to maintain existing behavior.
      // We should raise a JavaScript exception.
      result.l = NULL;
      break;
    case JavaType::TypeVoid:
      // Conversion to void must never happen.
      NOTREACHED();
      break;
  }
  return result;
}

jvalue CoerceJavaScriptValueToJavaValue(const NPVariant& variant,
                                        const JavaType& target_type) {
  // Note that in all these conversions, the relevant field of the jvalue must
  // always be explicitly set, as jvalue does not initialize its fields.

  // Some of these methods create new Java Strings. Note that we don't
  // explicitly release the local ref to these new objects, as there's no simple
  // way to do so.
  switch (variant.type) {
    case NPVariantType_Int32:
    case NPVariantType_Double:
      return CoerceJavaScriptNumberToJavaValue(variant, target_type);
    case NPVariantType_Bool:
      return CoerceJavaScriptBooleanToJavaValue(variant, target_type);
    case NPVariantType_String:
      return CoerceJavaScriptStringToJavaValue(variant, target_type);
    case NPVariantType_Object:
      return CoerceJavaScriptObjectToJavaValue(variant, target_type);
    case NPVariantType_Null:
    case NPVariantType_Void:
      return CoerceJavaScriptNullOrUndefinedToJavaValue(variant, target_type);
  }
  NOTREACHED();
  return jvalue();
}

class ObjectGetClassID : public MethodID {
 public:
  static ObjectGetClassID* GetInstance() {
    return Singleton<ObjectGetClassID>::get();
  }
 private:
  friend struct DefaultSingletonTraits<ObjectGetClassID>;
  ObjectGetClassID()
      : MethodID(AttachCurrentThread(), "java/lang/Object", "getClass",
                 "()Ljava/lang/Class;") {
  }
  DISALLOW_COPY_AND_ASSIGN(ObjectGetClassID);
};

class ClassGetMethodsID : public MethodID {
 public:
  static ClassGetMethodsID* GetInstance() {
    return Singleton<ClassGetMethodsID>::get();
  }
 private:
  friend struct DefaultSingletonTraits<ClassGetMethodsID>;
  ClassGetMethodsID()
      : MethodID(AttachCurrentThread(), "java/lang/Class", "getMethods",
                 "()[Ljava/lang/reflect/Method;") {
  }
  DISALLOW_COPY_AND_ASSIGN(ClassGetMethodsID);
};

}  // namespace


NPObject* JavaBoundObject::Create(const JavaRef<jobject>& object) {
  // The first argument (a plugin's instance handle) is passed through to the
  // allocate function directly, and we don't use it, so it's ok to be 0.
  // The object is created with a ref count of one.
  NPObject* np_object = WebBindings::createObject(0, const_cast<NPClass*>(
      &JavaNPObject::kNPClass));
  // The NPObject takes ownership of the JavaBoundObject.
  reinterpret_cast<JavaNPObject*>(np_object)->bound_object =
      new JavaBoundObject(object);
  return np_object;
}

JavaBoundObject::JavaBoundObject(const JavaRef<jobject>& object)
    : java_object_(object.env()->NewGlobalRef(object.obj())) {
  // We don't do anything with our Java object when first created. We do it all
  // lazily when a method is first invoked.
}

JavaBoundObject::~JavaBoundObject() {
  // Use the current thread's JNI env to release our global ref to the Java
  // object.
  AttachCurrentThread()->DeleteGlobalRef(java_object_);
}

jobject JavaBoundObject::GetJavaObject(NPObject* object) {
  DCHECK_EQ(&JavaNPObject::kNPClass, object->_class);
  JavaBoundObject* jbo = reinterpret_cast<JavaNPObject*>(object)->bound_object;
  return jbo->java_object_;
}

bool JavaBoundObject::HasMethod(const std::string& name) const {
  EnsureMethodsAreSetUp();
  return methods_.find(name) != methods_.end();
}

bool JavaBoundObject::Invoke(const std::string& name, const NPVariant* args,
                             size_t arg_count, NPVariant* result) {
  EnsureMethodsAreSetUp();

  // Get all methods with the correct name.
  std::pair<JavaMethodMap::const_iterator, JavaMethodMap::const_iterator>
      iters = methods_.equal_range(name);
  if (iters.first == iters.second) {
    return false;
  }

  // Take the first method with the correct number of arguments.
  JavaMethod* method = NULL;
  for (JavaMethodMap::const_iterator iter = iters.first; iter != iters.second;
       ++iter) {
    if (iter->second->num_parameters() == arg_count) {
      method = iter->second.get();
      break;
    }
  }
  if (!method) {
    return false;
  }

  // Coerce
  std::vector<jvalue> parameters(arg_count);
  for (size_t i = 0; i < arg_count; ++i) {
    parameters[i] = CoerceJavaScriptValueToJavaValue(args[i],
                                                     method->parameter_type(i));
  }

  // Call
  *result = CallJNIMethod(java_object_, method->return_type(), method->id(),
                          &parameters[0]);
  return true;
}

void JavaBoundObject::EnsureMethodsAreSetUp() const {
  if (!methods_.empty()) {
    return;
  }

  JNIEnv* env = AttachCurrentThread();
  ScopedJavaLocalRef<jclass> clazz(env, static_cast<jclass>(
      env->CallObjectMethod(java_object_,
                            ObjectGetClassID::GetInstance()->id())));
  ScopedJavaLocalRef<jobjectArray> methods(env, static_cast<jobjectArray>(
      env->CallObjectMethod(clazz.obj(),
                            ClassGetMethodsID::GetInstance()->id())));
  size_t num_methods = env->GetArrayLength(methods.obj());
  DCHECK(num_methods) << "Java objects always have public methods";
  for (size_t i = 0; i < num_methods; ++i) {
    ScopedJavaLocalRef<jobject> java_method(
        env,
        env->GetObjectArrayElement(methods.obj(), i));
    JavaMethod* method = new JavaMethod(java_method);
    methods_.insert(std::make_pair(method->name(), method));
  }
}
