// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cmath>

#include "base/memory/scoped_ptr.h"
#include "base/stl_util.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "content/renderer/v8_value_converter_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace content {

// To improve the performance of
// V8ValueConverterImpl::UpdateAndCheckUniqueness, identity hashes of objects
// are used during checking for duplicates. For testing purposes we need to
// ignore the hash sometimes. Create this helper object to avoid using identity
// hashes for the lifetime of the helper.
class ScopedAvoidIdentityHashForTesting {
 public:
  // The hashes will be ignored in |converter|, which must not be NULL and it
  // must outlive the created instance of this helper.
  explicit ScopedAvoidIdentityHashForTesting(
      content::V8ValueConverterImpl* converter);
  ~ScopedAvoidIdentityHashForTesting();

 private:
  content::V8ValueConverterImpl* converter_;

  DISALLOW_COPY_AND_ASSIGN(ScopedAvoidIdentityHashForTesting);
};

ScopedAvoidIdentityHashForTesting::ScopedAvoidIdentityHashForTesting(
    content::V8ValueConverterImpl* converter)
    : converter_(converter) {
  CHECK(converter_);
  converter_->avoid_identity_hash_for_testing_ = true;
}

ScopedAvoidIdentityHashForTesting::~ScopedAvoidIdentityHashForTesting() {
  converter_->avoid_identity_hash_for_testing_ = false;
}

namespace {

// A dumb getter for an object's named callback.
v8::Handle<v8::Value> NamedCallbackGetter(v8::Local<v8::String> name,
                                          const v8::AccessorInfo& info) {
  return v8::String::New("bar");
}

}  // namespace

class V8ValueConverterImplTest : public testing::Test {
 protected:
  virtual void SetUp() {
    v8::HandleScope handle_scope;
    v8::Handle<v8::ObjectTemplate> global = v8::ObjectTemplate::New();
    context_ = v8::Context::New(NULL, global);
  }

  virtual void TearDown() {
    context_.Dispose(context_->GetIsolate());
  }

  std::string GetString(base::DictionaryValue* value, const std::string& key) {
    std::string temp;
    if (!value->GetString(key, &temp)) {
      ADD_FAILURE();
      return "";
    }
    return temp;
  }

  std::string GetString(v8::Handle<v8::Object> value, const std::string& key) {
    v8::Handle<v8::String> temp =
        value->Get(v8::String::New(key.c_str())).As<v8::String>();
    if (temp.IsEmpty()) {
      ADD_FAILURE();
      return "";
    }
    v8::String::Utf8Value utf8(temp);
    return std::string(*utf8, utf8.length());
  }

  std::string GetString(base::ListValue* value, uint32 index) {
    std::string temp;
    if (!value->GetString(static_cast<size_t>(index), &temp)) {
      ADD_FAILURE();
      return "";
    }
    return temp;
  }

  std::string GetString(v8::Handle<v8::Array> value, uint32 index) {
    v8::Handle<v8::String> temp = value->Get(index).As<v8::String>();
    if (temp.IsEmpty()) {
      ADD_FAILURE();
      return "";
    }
    v8::String::Utf8Value utf8(temp);
    return std::string(*utf8, utf8.length());
  }

  bool IsNull(base::DictionaryValue* value, const std::string& key) {
    base::Value* child = NULL;
    if (!value->Get(key, &child)) {
      ADD_FAILURE();
      return false;
    }
    return child->GetType() == base::Value::TYPE_NULL;
  }

  bool IsNull(v8::Handle<v8::Object> value, const std::string& key) {
    v8::Handle<v8::Value> child = value->Get(v8::String::New(key.c_str()));
    if (child.IsEmpty()) {
      ADD_FAILURE();
      return false;
    }
    return child->IsNull();
  }

  bool IsNull(base::ListValue* value, uint32 index) {
    base::Value* child = NULL;
    if (!value->Get(static_cast<size_t>(index), &child)) {
      ADD_FAILURE();
      return false;
    }
    return child->GetType() == base::Value::TYPE_NULL;
  }

  bool IsNull(v8::Handle<v8::Array> value, uint32 index) {
    v8::Handle<v8::Value> child = value->Get(index);
    if (child.IsEmpty()) {
      ADD_FAILURE();
      return false;
    }
    return child->IsNull();
  }

  void TestWeirdType(const V8ValueConverterImpl& converter,
                     v8::Handle<v8::Value> val,
                     base::Value::Type expected_type,
                     scoped_ptr<base::Value> expected_value) {
    scoped_ptr<base::Value> raw(converter.FromV8Value(val, context_));

    if (expected_value.get()) {
      ASSERT_TRUE(raw.get());
      EXPECT_TRUE(expected_value->Equals(raw.get()));
      EXPECT_EQ(expected_type, raw->GetType());
    } else {
      EXPECT_FALSE(raw.get());
    }

    v8::Handle<v8::Object> object(v8::Object::New());
    object->Set(v8::String::New("test"), val);
    scoped_ptr<base::DictionaryValue> dictionary(
        static_cast<base::DictionaryValue*>(
            converter.FromV8Value(object, context_)));
    ASSERT_TRUE(dictionary.get());

    if (expected_value.get()) {
      base::Value* temp = NULL;
      ASSERT_TRUE(dictionary->Get("test", &temp));
      EXPECT_EQ(expected_type, temp->GetType());
      EXPECT_TRUE(expected_value->Equals(temp));
    } else {
      EXPECT_FALSE(dictionary->HasKey("test"));
    }

    v8::Handle<v8::Array> array(v8::Array::New());
    array->Set(0, val);
    scoped_ptr<base::ListValue> list(
        static_cast<base::ListValue*>(converter.FromV8Value(array, context_)));
    ASSERT_TRUE(list.get());
    if (expected_value.get()) {
      base::Value* temp = NULL;
      ASSERT_TRUE(list->Get(0, &temp));
      EXPECT_EQ(expected_type, temp->GetType());
      EXPECT_TRUE(expected_value->Equals(temp));
    } else {
      // Arrays should preserve their length, and convert unconvertible
      // types into null.
      base::Value* temp = NULL;
      ASSERT_TRUE(list->Get(0, &temp));
      EXPECT_EQ(base::Value::TYPE_NULL, temp->GetType());
    }
  }

  // Context for the JavaScript in the test.
  v8::Persistent<v8::Context> context_;
};

TEST_F(V8ValueConverterImplTest, BasicRoundTrip) {
  scoped_ptr<base::Value> original_root = base::test::ParseJson(
      "{ \n"
      "  \"null\": null, \n"
      "  \"true\": true, \n"
      "  \"false\": false, \n"
      "  \"positive-int\": 42, \n"
      "  \"negative-int\": -42, \n"
      "  \"zero\": 0, \n"
      "  \"double\": 88.8, \n"
      "  \"big-integral-double\": 9007199254740992.0, \n"  // 2.0^53
      "  \"string\": \"foobar\", \n"
      "  \"empty-string\": \"\", \n"
      "  \"dictionary\": { \n"
      "    \"foo\": \"bar\",\n"
      "    \"hot\": \"dog\",\n"
      "  }, \n"
      "  \"empty-dictionary\": {}, \n"
      "  \"list\": [ \"monkey\", \"balls\" ], \n"
      "  \"empty-list\": [], \n"
      "}");

  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  V8ValueConverterImpl converter;
  v8::Handle<v8::Object> v8_object =
      converter.ToV8Value(original_root.get(), context_).As<v8::Object>();
  ASSERT_FALSE(v8_object.IsEmpty());

  EXPECT_EQ(static_cast<const base::DictionaryValue&>(*original_root).size(),
            v8_object->GetPropertyNames()->Length());
  EXPECT_TRUE(v8_object->Get(v8::String::New("null"))->IsNull());
  EXPECT_TRUE(v8_object->Get(v8::String::New("true"))->IsTrue());
  EXPECT_TRUE(v8_object->Get(v8::String::New("false"))->IsFalse());
  EXPECT_TRUE(v8_object->Get(v8::String::New("positive-int"))->IsInt32());
  EXPECT_TRUE(v8_object->Get(v8::String::New("negative-int"))->IsInt32());
  EXPECT_TRUE(v8_object->Get(v8::String::New("zero"))->IsInt32());
  EXPECT_TRUE(v8_object->Get(v8::String::New("double"))->IsNumber());
  EXPECT_TRUE(
      v8_object->Get(v8::String::New("big-integral-double"))->IsNumber());
  EXPECT_TRUE(v8_object->Get(v8::String::New("string"))->IsString());
  EXPECT_TRUE(v8_object->Get(v8::String::New("empty-string"))->IsString());
  EXPECT_TRUE(v8_object->Get(v8::String::New("dictionary"))->IsObject());
  EXPECT_TRUE(v8_object->Get(v8::String::New("empty-dictionary"))->IsObject());
  EXPECT_TRUE(v8_object->Get(v8::String::New("list"))->IsArray());
  EXPECT_TRUE(v8_object->Get(v8::String::New("empty-list"))->IsArray());

  scoped_ptr<base::Value> new_root(converter.FromV8Value(v8_object, context_));
  EXPECT_NE(original_root.get(), new_root.get());
  EXPECT_TRUE(original_root->Equals(new_root.get()));
}

TEST_F(V8ValueConverterImplTest, KeysWithDots) {
  scoped_ptr<base::Value> original =
      base::test::ParseJson("{ \"foo.bar\": \"baz\" }");

  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  V8ValueConverterImpl converter;
  scoped_ptr<base::Value> copy(
      converter.FromV8Value(
          converter.ToV8Value(original.get(), context_), context_));

  EXPECT_TRUE(original->Equals(copy.get()));
}

TEST_F(V8ValueConverterImplTest, ObjectExceptions) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  // Set up objects to throw when reading or writing 'foo'.
  const char* source =
      "Object.prototype.__defineSetter__('foo', "
      "    function() { throw new Error('muah!'); });"
      "Object.prototype.__defineGetter__('foo', "
      "    function() { throw new Error('muah!'); });";

  v8::Handle<v8::Script> script(v8::Script::New(v8::String::New(source)));
  script->Run();

  v8::Handle<v8::Object> object(v8::Object::New());
  object->Set(v8::String::New("bar"), v8::String::New("bar"));

  // Converting from v8 value should replace the foo property with null.
  V8ValueConverterImpl converter;
  scoped_ptr<base::DictionaryValue> converted(
      static_cast<base::DictionaryValue*>(
          converter.FromV8Value(object, context_)));
  EXPECT_TRUE(converted.get());
  // http://code.google.com/p/v8/issues/detail?id=1342
  // EXPECT_EQ(2u, converted->size());
  // EXPECT_TRUE(IsNull(converted.get(), "foo"));
  EXPECT_EQ(1u, converted->size());
  EXPECT_EQ("bar", GetString(converted.get(), "bar"));

  // Converting to v8 value should drop the foo property.
  converted->SetString("foo", "foo");
  v8::Handle<v8::Object> copy =
      converter.ToV8Value(converted.get(), context_).As<v8::Object>();
  EXPECT_FALSE(copy.IsEmpty());
  EXPECT_EQ(2u, copy->GetPropertyNames()->Length());
  EXPECT_EQ("bar", GetString(copy, "bar"));
}

TEST_F(V8ValueConverterImplTest, ArrayExceptions) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  const char* source = "(function() {"
      "var arr = [];"
      "arr.__defineSetter__(0, "
      "    function() { throw new Error('muah!'); });"
      "arr.__defineGetter__(0, "
      "    function() { throw new Error('muah!'); });"
      "arr[1] = 'bar';"
      "return arr;"
      "})();";

  v8::Handle<v8::Script> script(v8::Script::New(v8::String::New(source)));
  v8::Handle<v8::Array> array = script->Run().As<v8::Array>();
  ASSERT_FALSE(array.IsEmpty());

  // Converting from v8 value should replace the first item with null.
  V8ValueConverterImpl converter;
  scoped_ptr<base::ListValue> converted(static_cast<base::ListValue*>(
      converter.FromV8Value(array, context_)));
  ASSERT_TRUE(converted.get());
  // http://code.google.com/p/v8/issues/detail?id=1342
  EXPECT_EQ(2u, converted->GetSize());
  EXPECT_TRUE(IsNull(converted.get(), 0));

  // Converting to v8 value should drop the first item and leave a hole.
  converted.reset(static_cast<base::ListValue*>(
      base::test::ParseJson("[ \"foo\", \"bar\" ]").release()));
  v8::Handle<v8::Array> copy =
      converter.ToV8Value(converted.get(), context_).As<v8::Array>();
  ASSERT_FALSE(copy.IsEmpty());
  EXPECT_EQ(2u, copy->Length());
  EXPECT_EQ("bar", GetString(copy, 1));
}

TEST_F(V8ValueConverterImplTest, WeirdTypes) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  v8::Handle<v8::RegExp> regex(
      v8::RegExp::New(v8::String::New("."), v8::RegExp::kNone));

  V8ValueConverterImpl converter;
  TestWeirdType(converter,
                v8::Undefined(),
                base::Value::TYPE_NULL,  // Arbitrary type, result is NULL.
                scoped_ptr<base::Value>(NULL));
  TestWeirdType(converter,
                v8::Date::New(1000),
                base::Value::TYPE_DICTIONARY,
                scoped_ptr<base::Value>(new base::DictionaryValue()));
  TestWeirdType(converter,
                regex,
                base::Value::TYPE_DICTIONARY,
                scoped_ptr<base::Value>(new base::DictionaryValue()));

  converter.SetDateAllowed(true);
  TestWeirdType(converter,
                v8::Date::New(1000),
                base::Value::TYPE_DOUBLE,
                scoped_ptr<base::Value>(new base::FundamentalValue(1.0)));

  converter.SetRegExpAllowed(true);
  TestWeirdType(converter,
                regex,
                base::Value::TYPE_STRING,
                scoped_ptr<base::Value>(new base::StringValue("/./")));
}

TEST_F(V8ValueConverterImplTest, Prototype) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  const char* source = "(function() {"
      "Object.prototype.foo = 'foo';"
      "return {};"
      "})();";

  v8::Handle<v8::Script> script(v8::Script::New(v8::String::New(source)));
  v8::Handle<v8::Object> object = script->Run().As<v8::Object>();
  ASSERT_FALSE(object.IsEmpty());

  V8ValueConverterImpl converter;
  scoped_ptr<base::DictionaryValue> result(
      static_cast<base::DictionaryValue*>(
          converter.FromV8Value(object, context_)));
  ASSERT_TRUE(result.get());
  EXPECT_EQ(0u, result->size());
}

TEST_F(V8ValueConverterImplTest, StripNullFromObjects) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  const char* source = "(function() {"
      "return { foo: undefined, bar: null };"
      "})();";

  v8::Handle<v8::Script> script(v8::Script::New(v8::String::New(source)));
  v8::Handle<v8::Object> object = script->Run().As<v8::Object>();
  ASSERT_FALSE(object.IsEmpty());

  V8ValueConverterImpl converter;
  converter.SetStripNullFromObjects(true);

  scoped_ptr<base::DictionaryValue> result(
      static_cast<base::DictionaryValue*>(
          converter.FromV8Value(object, context_)));
  ASSERT_TRUE(result.get());
  EXPECT_EQ(0u, result->size());
}

TEST_F(V8ValueConverterImplTest, RecursiveObjects) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  V8ValueConverterImpl converter;

  v8::Handle<v8::Object> object = v8::Object::New().As<v8::Object>();
  ASSERT_FALSE(object.IsEmpty());
  object->Set(v8::String::New("foo"), v8::String::New("bar"));
  object->Set(v8::String::New("obj"), object);

  scoped_ptr<base::DictionaryValue> object_result(
      static_cast<base::DictionaryValue*>(
          converter.FromV8Value(object, context_)));
  ASSERT_TRUE(object_result.get());
  EXPECT_EQ(2u, object_result->size());
  EXPECT_TRUE(IsNull(object_result.get(), "obj"));

  v8::Handle<v8::Array> array = v8::Array::New().As<v8::Array>();
  ASSERT_FALSE(array.IsEmpty());
  array->Set(0, v8::String::New("1"));
  array->Set(1, array);

  scoped_ptr<base::ListValue> list_result(
      static_cast<base::ListValue*>(converter.FromV8Value(array, context_)));
  ASSERT_TRUE(list_result.get());
  EXPECT_EQ(2u, list_result->GetSize());
  EXPECT_TRUE(IsNull(list_result.get(), 1));
}

// Do not try and convert any named callbacks including getters.
TEST_F(V8ValueConverterImplTest, ObjectGetters) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  const char* source = "(function() {"
      "var a = {};"
      "a.__defineGetter__('foo', function() { return 'bar'; });"
      "return a;"
      "})();";

  v8::Handle<v8::Script> script(v8::Script::New(v8::String::New(source)));
  v8::Handle<v8::Object> object = script->Run().As<v8::Object>();
  ASSERT_FALSE(object.IsEmpty());

  V8ValueConverterImpl converter;
  scoped_ptr<base::DictionaryValue> result(
      static_cast<base::DictionaryValue*>(
          converter.FromV8Value(object, context_)));
  ASSERT_TRUE(result.get());
  EXPECT_EQ(0u, result->size());
}

// Do not try and convert any named callbacks including getters.
TEST_F(V8ValueConverterImplTest, ObjectWithInternalFieldsGetters) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  v8::Handle<v8::ObjectTemplate> object_template = v8::ObjectTemplate::New();
  object_template->SetInternalFieldCount(1);
  object_template->SetAccessor(v8::String::New("foo"), NamedCallbackGetter);
  v8::Handle<v8::Object> object = object_template->NewInstance();
  ASSERT_FALSE(object.IsEmpty());
  object->Set(v8::String::New("a"), v8::String::New("b"));

  V8ValueConverterImpl converter;
  scoped_ptr<base::DictionaryValue> result(
      static_cast<base::DictionaryValue*>(
          converter.FromV8Value(object, context_)));
  ASSERT_TRUE(result.get());
  EXPECT_EQ(1u, result->size());
}

TEST_F(V8ValueConverterImplTest, WeirdProperties) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

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

  V8ValueConverterImpl converter;
  scoped_ptr<base::Value> actual(converter.FromV8Value(object, context_));

  scoped_ptr<base::Value> expected = base::test::ParseJson(
      "{ \n"
      "  \"1\": \"foo\", \n"
      "  \"2\": \"bar\", \n"
      "  \"true\": \"baz\", \n"
      "  \"false\": \"qux\", \n"
      "  \"null\": \"quux\", \n"
      "  \"undefined\": \"oops\", \n"
      "}");

  EXPECT_TRUE(expected->Equals(actual.get()));
}

TEST_F(V8ValueConverterImplTest, ArrayGetters) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  const char* source = "(function() {"
      "var a = [0];"
      "a.__defineGetter__(1, function() { return 'bar'; });"
      "return a;"
      "})();";

  v8::Handle<v8::Script> script(v8::Script::New(v8::String::New(source)));
  v8::Handle<v8::Array> array = script->Run().As<v8::Array>();
  ASSERT_FALSE(array.IsEmpty());

  V8ValueConverterImpl converter;
  scoped_ptr<base::ListValue> result(
      static_cast<base::ListValue*>(converter.FromV8Value(array, context_)));
  ASSERT_TRUE(result.get());
  EXPECT_EQ(2u, result->GetSize());
}

TEST_F(V8ValueConverterImplTest, UndefinedValueBehavior) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;

  v8::Handle<v8::Object> object;
  {
    const char* source = "(function() {"
        "return { foo: undefined, bar: null, baz: function(){} };"
        "})();";
    v8::Handle<v8::Script> script(v8::Script::New(v8::String::New(source)));
    object = script->Run().As<v8::Object>();
    ASSERT_FALSE(object.IsEmpty());
  }

  v8::Handle<v8::Array> array;
  {
    const char* source = "(function() {"
        "return [ undefined, null, function(){} ];"
        "})();";
    v8::Handle<v8::Script> script(v8::Script::New(v8::String::New(source)));
    array = script->Run().As<v8::Array>();
    ASSERT_FALSE(array.IsEmpty());
  }

  V8ValueConverterImpl converter;

  scoped_ptr<base::Value> actual_object(
      converter.FromV8Value(object, context_));
  EXPECT_TRUE(base::Value::Equals(
      base::test::ParseJson("{ \"bar\": null }").get(), actual_object.get()));

  // Everything is null because JSON stringification preserves array length.
  scoped_ptr<Value> actual_array(converter.FromV8Value(array, context_));
  EXPECT_TRUE(base::Value::Equals(
      base::test::ParseJson("[ null, null, null ]").get(), actual_array.get()));
}

TEST_F(V8ValueConverterImplTest, ObjectsWithClashingIdentityHash) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;
  V8ValueConverterImpl converter;

  // We check that the converter checks identity correctly by disabling the
  // optimization of using identity hashes.
  ScopedAvoidIdentityHashForTesting scoped_hash_avoider(&converter);

  // Create the v8::Object to be converted.
  v8::Handle<v8::Array> root(v8::Array::New(4));
  root->Set(0, v8::Handle<v8::Object>(v8::Object::New()));
  root->Set(1, v8::Handle<v8::Object>(v8::Object::New()));
  root->Set(2, v8::Handle<v8::Object>(v8::Array::New(0)));
  root->Set(3, v8::Handle<v8::Object>(v8::Array::New(0)));

  // The expected base::Value result.
  scoped_ptr<base::Value> expected = base::test::ParseJson("[{},{},[],[]]");
  ASSERT_TRUE(expected.get());

  // The actual result.
  scoped_ptr<base::Value> value(converter.FromV8Value(root, context_));
  ASSERT_TRUE(value.get());

  EXPECT_TRUE(expected->Equals(value.get()));
}

TEST_F(V8ValueConverterImplTest, DetectCycles) {
  v8::Context::Scope context_scope(context_);
  v8::HandleScope handle_scope;
  V8ValueConverterImpl converter;

  // Create a recursive array.
  v8::Handle<v8::Array> recursive_array(v8::Array::New(1));
  recursive_array->Set(0, recursive_array);

  // The first repetition should be trimmed and replaced by a null value.
  base::ListValue expected_list;
  expected_list.Append(base::Value::CreateNullValue());

  // The actual result.
  scoped_ptr<base::Value> actual_list(
      converter.FromV8Value(recursive_array, context_));
  ASSERT_TRUE(actual_list.get());

  EXPECT_TRUE(expected_list.Equals(actual_list.get()));

  // Now create a recursive object
  const std::string key("key");
  v8::Handle<v8::Object> recursive_object(v8::Object::New());
  v8::TryCatch try_catch;
  recursive_object->Set(v8::String::New(key.c_str(), key.length()),
                        recursive_object);
  ASSERT_FALSE(try_catch.HasCaught());

  // The first repetition should be trimmed and replaced by a null value.
  base::DictionaryValue expected_dictionary;
  expected_dictionary.Set(key, base::Value::CreateNullValue());

  // The actual result.
  scoped_ptr<base::Value> actual_dictionary(
      converter.FromV8Value(recursive_object, context_));
  ASSERT_TRUE(actual_dictionary.get());

  EXPECT_TRUE(expected_dictionary.Equals(actual_dictionary.get()));
}

}  // namespace content
