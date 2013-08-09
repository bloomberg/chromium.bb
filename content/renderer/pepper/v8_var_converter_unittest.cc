// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/v8_var_converter.h"

#include <cmath>

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/shared_impl/array_var.h"
#include "ppapi/shared_impl/dictionary_var.h"
#include "ppapi/shared_impl/ppapi_globals.h"
#include "ppapi/shared_impl/proxy_lock.h"
#include "ppapi/shared_impl/scoped_pp_var.h"
#include "ppapi/shared_impl/test_globals.h"
#include "ppapi/shared_impl/unittest_utils.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

using ppapi::ArrayBufferVar;
using ppapi::ArrayVar;
using ppapi::DictionaryVar;
using ppapi::PpapiGlobals;
using ppapi::ProxyLock;
using ppapi::ScopedPPVar;
using ppapi::StringVar;
using ppapi::TestGlobals;
using ppapi::TestEqual;
using ppapi::VarTracker;

namespace content {

namespace {

// Maps PP_Var IDs to the V8 value handle they correspond to.
typedef base::hash_map<int64_t, v8::Handle<v8::Value> > VarHandleMap;

bool Equals(const PP_Var& var,
            v8::Handle<v8::Value> val,
            VarHandleMap* visited_ids) {
  if (ppapi::VarTracker::IsVarTypeRefcounted(var.type)) {
    VarHandleMap::iterator it = visited_ids->find(var.value.as_id);
    if (it != visited_ids->end())
      return it->second == val;
    (*visited_ids)[var.value.as_id] = val;
  }

  if (val->IsUndefined()) {
    return var.type == PP_VARTYPE_UNDEFINED;
  } else if (val->IsNull()) {
    return var.type == PP_VARTYPE_NULL;
  } else if (val->IsBoolean() || val->IsBooleanObject()) {
    return var.type == PP_VARTYPE_BOOL &&
        PP_FromBool(val->ToBoolean()->Value()) == var.value.as_bool;
  } else if (val->IsInt32()) {
    return var.type == PP_VARTYPE_INT32 &&
        val->ToInt32()->Value() == var.value.as_int;
  } else if (val->IsNumber() || val->IsNumberObject()) {
    return var.type == PP_VARTYPE_DOUBLE &&
        fabs(val->ToNumber()->Value() - var.value.as_double) <= 1.0e-4;
  } else if (val->IsString() || val->IsStringObject()) {
    if (var.type != PP_VARTYPE_STRING)
      return false;
    StringVar* string_var = StringVar::FromPPVar(var);
    DCHECK(string_var);
    v8::String::Utf8Value utf8(val->ToString());
    return std::string(*utf8, utf8.length()) == string_var->value();
  } else if (val->IsArray()) {
    if (var.type != PP_VARTYPE_ARRAY)
      return false;
    ArrayVar* array_var = ArrayVar::FromPPVar(var);
    DCHECK(array_var);
    v8::Handle<v8::Array> v8_array = val.As<v8::Array>();
    if (v8_array->Length() != array_var->elements().size())
      return false;
    for (uint32 i = 0; i < v8_array->Length(); ++i) {
      v8::Handle<v8::Value> child_v8 = v8_array->Get(i);
      if (!Equals(array_var->elements()[i].get(), child_v8, visited_ids))
        return false;
    }
    return true;
  } else if (val->IsObject()) {
    if (var.type == PP_VARTYPE_ARRAY_BUFFER) {
      // TODO(raymes): Implement this when we have tests for array buffers.
      NOTIMPLEMENTED();
      return false;
    } else {
      v8::Handle<v8::Object> v8_object = val->ToObject();
      if (var.type != PP_VARTYPE_DICTIONARY)
        return false;
      DictionaryVar* dict_var = DictionaryVar::FromPPVar(var);
      DCHECK(dict_var);
      v8::Handle<v8::Array> property_names(v8_object->GetOwnPropertyNames());
      if (property_names->Length() != dict_var->key_value_map().size())
        return false;
      for (uint32 i = 0; i < property_names->Length(); ++i) {
        v8::Handle<v8::Value> key(property_names->Get(i));

        if (!key->IsString() && !key->IsNumber())
          return false;
        v8::Handle<v8::Value> child_v8 = v8_object->Get(key);

        v8::String::Utf8Value name_utf8(key->ToString());
        ScopedPPVar release_key(ScopedPPVar::PassRef(),
            StringVar::StringToPPVar(
                std::string(*name_utf8, name_utf8.length())));
        if (!dict_var->HasKey(release_key.get()))
          return false;
        ScopedPPVar release_value(ScopedPPVar::PassRef(),
                                  dict_var->Get(release_key.get()));
        if (!Equals(release_value.get(), child_v8, visited_ids))
          return false;
      }
      return true;
    }
  }
  return false;
}

bool Equals(const PP_Var& var,
            v8::Handle<v8::Value> val) {
  VarHandleMap var_handle_map;
  return Equals(var, val, &var_handle_map);
}

class V8VarConverterTest : public testing::Test {
 public:
  V8VarConverterTest()
      : isolate_(v8::Isolate::GetCurrent()) {}
  virtual ~V8VarConverterTest() {}

  // testing::Test implementation.
  virtual void SetUp() {
    ProxyLock::Acquire();
    v8::HandleScope handle_scope(isolate_);
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
    context_.Reset(isolate_, v8::Context::New(isolate_, NULL, global));
  }
  virtual void TearDown() {
    context_.Dispose();
    ASSERT_TRUE(PpapiGlobals::Get()->GetVarTracker()->GetLiveVars().empty());
    ProxyLock::Release();
  }

 protected:
  bool RoundTrip(const PP_Var& var, PP_Var* result) {
    v8::HandleScope handle_scope(isolate_);
    v8::Context::Scope context_scope(isolate_, context_);
    v8::Local<v8::Context> context =
        v8::Local<v8::Context>::New(isolate_, context_);
    v8::Handle<v8::Value> v8_result;
    if (!V8VarConverter::ToV8Value(var, context, &v8_result))
      return false;
    if (!Equals(var, v8_result))
      return false;
    if (!V8VarConverter::FromV8Value(v8_result, context, result))
      return false;
    return true;
  }

  // Assumes a ref for var.
  bool RoundTripAndCompare(const PP_Var& var) {
    ScopedPPVar expected(ScopedPPVar::PassRef(), var);
    PP_Var actual_var;
    if (!RoundTrip(expected.get(), &actual_var))
      return false;
    ScopedPPVar actual(ScopedPPVar::PassRef(), actual_var);
    return TestEqual(expected.get(), actual.get());
  }

  v8::Isolate* isolate_;

  // Context for the JavaScript in the test.
  v8::Persistent<v8::Context> context_;

 private:
  TestGlobals globals_;
};

}  // namespace

TEST_F(V8VarConverterTest, SimpleRoundTripTest) {
  EXPECT_TRUE(RoundTripAndCompare(PP_MakeUndefined()));
  EXPECT_TRUE(RoundTripAndCompare(PP_MakeNull()));
  EXPECT_TRUE(RoundTripAndCompare(PP_MakeInt32(100)));
  EXPECT_TRUE(RoundTripAndCompare(PP_MakeBool(PP_TRUE)));
  EXPECT_TRUE(RoundTripAndCompare(PP_MakeDouble(53.75)));
}

TEST_F(V8VarConverterTest, StringRoundTripTest) {
  EXPECT_TRUE(RoundTripAndCompare(StringVar::StringToPPVar("")));
  EXPECT_TRUE(RoundTripAndCompare(StringVar::StringToPPVar("hello world!")));
}

TEST_F(V8VarConverterTest, ArrayBufferRoundTripTest) {
  // TODO(raymes): Testing this here requires spinning up some of WebKit.
  // Work out how to do this.
}

TEST_F(V8VarConverterTest, DictionaryArrayRoundTripTest) {
  // Empty array.
  scoped_refptr<ArrayVar> array(new ArrayVar);
  ScopedPPVar release_array(ScopedPPVar::PassRef(), array->GetPPVar());
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));

  size_t index = 0;

  // Array with primitives.
  array->Set(index++, PP_MakeUndefined());
  array->Set(index++, PP_MakeNull());
  array->Set(index++, PP_MakeInt32(100));
  array->Set(index++, PP_MakeBool(PP_FALSE));
  array->Set(index++, PP_MakeDouble(0.123));
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));

  // Array with 2 references to the same string.
  ScopedPPVar release_string(
      ScopedPPVar::PassRef(), StringVar::StringToPPVar("abc"));
  array->Set(index++, release_string.get());
  array->Set(index++, release_string.get());
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));

  // Array with nested array that references the same string.
  scoped_refptr<ArrayVar> array2(new ArrayVar);
  ScopedPPVar release_array2(ScopedPPVar::PassRef(), array2->GetPPVar());
  array2->Set(0, release_string.get());
  array->Set(index++, release_array2.get());
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));

  // Empty dictionary.
  scoped_refptr<DictionaryVar> dictionary(new DictionaryVar);
  ScopedPPVar release_dictionary(ScopedPPVar::PassRef(),
                                 dictionary->GetPPVar());
  EXPECT_TRUE(RoundTripAndCompare(dictionary->GetPPVar()));

  // Dictionary with primitives.
  dictionary->SetWithStringKey("1", PP_MakeUndefined());
  dictionary->SetWithStringKey("2", PP_MakeNull());
  dictionary->SetWithStringKey("3", PP_MakeInt32(-100));
  dictionary->SetWithStringKey("4", PP_MakeBool(PP_TRUE));
  dictionary->SetWithStringKey("5", PP_MakeDouble(-103.52));
  EXPECT_TRUE(RoundTripAndCompare(dictionary->GetPPVar()));

  // Dictionary with 2 references to the same string.
  dictionary->SetWithStringKey("6", release_string.get());
  dictionary->SetWithStringKey("7", release_string.get());
  EXPECT_TRUE(RoundTripAndCompare(dictionary->GetPPVar()));

  // Dictionary with nested dictionary that references the same string.
  scoped_refptr<DictionaryVar> dictionary2(new DictionaryVar);
  ScopedPPVar release_dictionary2(ScopedPPVar::PassRef(),
                                  dictionary2->GetPPVar());
  dictionary2->SetWithStringKey("abc", release_string.get());
  dictionary->SetWithStringKey("8", release_dictionary2.get());
  EXPECT_TRUE(RoundTripAndCompare(dictionary->GetPPVar()));

  // Array with dictionary.
  array->Set(index++, release_dictionary.get());
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));

  // Array with dictionary with array.
  array2->Set(0, PP_MakeInt32(100));
  dictionary->SetWithStringKey("9", release_array2.get());
  EXPECT_TRUE(RoundTripAndCompare(array->GetPPVar()));
}

TEST_F(V8VarConverterTest, Cycles) {
  // Check that cycles aren't converted.
  v8::HandleScope handle_scope(isolate_);
  v8::Context::Scope context_scope(isolate_, context_);
  v8::Local<v8::Context> context =
      v8::Local<v8::Context>::New(isolate_, context_);

  // Var->V8 conversion.
  {
    scoped_refptr<DictionaryVar> dictionary(new DictionaryVar);
    ScopedPPVar release_dictionary(ScopedPPVar::PassRef(),
                                   dictionary->GetPPVar());
    scoped_refptr<ArrayVar> array(new ArrayVar);
    ScopedPPVar release_array(ScopedPPVar::PassRef(), array->GetPPVar());

    dictionary->SetWithStringKey("1", release_array.get());
    array->Set(0, release_dictionary.get());

    v8::Handle<v8::Value> v8_result;

    // Array <-> dictionary cycle.
    dictionary->SetWithStringKey("1", release_array.get());
    ASSERT_FALSE(V8VarConverter::ToV8Value(release_dictionary.get(),
                                           context, &v8_result));
    // Break the cycle.
    // TODO(raymes): We need some better machinery for releasing vars with
    // cycles. Remove the code below once we have that.
    dictionary->DeleteWithStringKey("1");

    // Array with self reference.
    array->Set(0, release_array.get());
    ASSERT_FALSE(V8VarConverter::ToV8Value(release_array.get(),
                                           context, &v8_result));
    // Break the self reference.
    array->Set(0, PP_MakeUndefined());
  }

  // V8->Var conversion.
  {
    v8::Handle<v8::Object> object = v8::Object::New();
    v8::Handle<v8::Array> array = v8::Array::New();

    PP_Var var_result;

    // Array <-> dictionary cycle.
    std::string key = "1";
    object->Set(v8::String::New(key.c_str(), key.length()), array);
    array->Set(0, object);

    ASSERT_FALSE(V8VarConverter::FromV8Value(object, context, &var_result));

    // Array with self reference.
    array->Set(0, array);
    ASSERT_FALSE(V8VarConverter::FromV8Value(array, context, &var_result));
  }
}

TEST_F(V8VarConverterTest, StrangeDictionaryKeyTest) {
  {
    // Test keys with '.'.
    scoped_refptr<DictionaryVar> dictionary(new DictionaryVar);
    dictionary->SetWithStringKey(".", PP_MakeUndefined());
    dictionary->SetWithStringKey("x.y", PP_MakeUndefined());
    EXPECT_TRUE(RoundTripAndCompare(dictionary->GetPPVar()));
  }

  {
    // Test non-string key types. They should be cast to strings.
    v8::HandleScope handle_scope(isolate_);
    v8::Context::Scope context_scope(isolate_, context_);

    const char* source = "(function() {"
        "return {"
          "1: 'foo',"
          "'2': 'bar',"
          "true: 'baz',"
          "false: 'qux',"
          "null: 'quux',"
          "undefined: 'oops'"
        "};"
        "})();";

    v8::Handle<v8::Script> script(v8::Script::New(v8::String::New(source)));
    v8::Handle<v8::Object> object = script->Run().As<v8::Object>();
    ASSERT_FALSE(object.IsEmpty());

    PP_Var actual;
    ASSERT_TRUE(V8VarConverter::FromV8Value(object,
        v8::Local<v8::Context>::New(isolate_, context_), &actual));
    ScopedPPVar release_actual(ScopedPPVar::PassRef(), actual);

    scoped_refptr<DictionaryVar> expected(new DictionaryVar);
    ScopedPPVar foo(ScopedPPVar::PassRef(), StringVar::StringToPPVar("foo"));
    expected->SetWithStringKey("1", foo.get());
    ScopedPPVar bar(ScopedPPVar::PassRef(), StringVar::StringToPPVar("bar"));
    expected->SetWithStringKey("2", bar.get());
    ScopedPPVar baz(ScopedPPVar::PassRef(), StringVar::StringToPPVar("baz"));
    expected->SetWithStringKey("true", baz.get());
    ScopedPPVar qux(ScopedPPVar::PassRef(), StringVar::StringToPPVar("qux"));
    expected->SetWithStringKey("false", qux.get());
    ScopedPPVar quux(ScopedPPVar::PassRef(), StringVar::StringToPPVar("quux"));
    expected->SetWithStringKey("null", quux.get());
    ScopedPPVar oops(ScopedPPVar::PassRef(), StringVar::StringToPPVar("oops"));
    expected->SetWithStringKey("undefined", oops.get());
    ScopedPPVar release_expected(
        ScopedPPVar::PassRef(), expected->GetPPVar());

    ASSERT_TRUE(TestEqual(release_expected.get(), release_actual.get()));
  }
}

}  // namespace content
