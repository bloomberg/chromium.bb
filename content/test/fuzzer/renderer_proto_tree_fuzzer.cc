// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzzer for content/renderer

#include <stddef.h>
#include <stdint.h>
#include <memory>
#include <sstream>

#include "content/test/fuzzer/fuzzer_support.h"
#include "content/test/fuzzer/html_tree.pb.h"
#include "third_party/libprotobuf-mutator/src/src/binary_format.h"
#include "third_party/libprotobuf-mutator/src/src/libfuzzer/libfuzzer_mutator.h"

protobuf_mutator::protobuf::LogSilencer log_silincer;

namespace content {

class HtmlTreeWriter {
 public:
  HtmlTreeWriter() {}

  template <typename T>
  HtmlTreeWriter& operator<<(const T& t) {
    out_ << t;
    return *this;
  }

  std::string str() const { return out_.str(); }

 private:
  std::ostringstream out_;
};

static HtmlTreeWriter& operator<<(HtmlTreeWriter& w,
                                  const Attribute::Value& value) {
  switch (value.value_case()) {
    case Attribute::Value::kBoolValue:
      return w << (value.bool_value() ? "true" : "false");
    case Attribute::Value::kUintValue:
      return w << value.uint_value();
    case Attribute::Value::kIntValue:
      return w << value.int_value();
    case Attribute::Value::kDoubleValue:
      return w << value.double_value();
    case Attribute::Value::kPxValue:
      return w << value.px_value() << "px";
    case Attribute::Value::kPctValue:
      return w << value.pct_value() << "%";
    case Attribute::Value::VALUE_NOT_SET:
      return w;
  }
}

static HtmlTreeWriter& operator<<(HtmlTreeWriter& w,
                                  const Attribute::Name& name) {
  return w << Attribute_Name_Name(name);
}

static HtmlTreeWriter& operator<<(HtmlTreeWriter& w, const Attribute& attr) {
  return w << attr.name() << "=\"" << attr.value() << "\"";
}

static HtmlTreeWriter& operator<<(HtmlTreeWriter& w, const Tag::Name& tagName) {
  return w << Tag_Name_Name(tagName);
}

static void operator<<(HtmlTreeWriter& w, const Tag& tag) {
  w << "<" << tag.name();
  for (const auto& attr : tag.attrs()) {
    w << " " << attr;
  }

  w << ">";
  for (const auto& subtag : tag.subtags()) {
    w << subtag;
  }
  w << "</" << tag.name() << ">";
}

static void operator<<(HtmlTreeWriter& w, const Document& document) {
  w << document.root();
}

static std::string str(const uint8_t* data, size_t size) {
  Document document;
  protobuf_mutator::ParseBinaryMessage(data, size, &document);

  HtmlTreeWriter writer;
  writer << document;
  return writer.str();
  //  return document.ShortDebugString();
}

extern "C" void LLVMPrintInput(const uint8_t* data, size_t size) {
  //  fprintf(stderr, "NEW %s\n", str(data, size).c_str());
}

extern "C" size_t LLVMFuzzerCustomMutator(uint8_t* data,
                                          size_t size,
                                          size_t max_size,
                                          unsigned int seed) {
  fprintf(stderr, "BEFORE    %s\n", str(data, size).c_str());
  size_t new_size = protobuf_mutator::libfuzzer::MutateBinaryMessage<Document>(
      data, size, max_size, seed);
  fprintf(stderr, "AFTER     %s\n", str(data, new_size).c_str());
  return new_size;
}

extern "C" size_t LLVMFuzzerCustomCrossOver(const uint8_t* data1,
                                            size_t size1,
                                            const uint8_t* data2,
                                            size_t size2,
                                            uint8_t* out,
                                            size_t max_out_size,
                                            unsigned int seed) {
  fprintf(stderr, "BEFOR1    %s\n", str(data1, size1).c_str());
  fprintf(stderr, "BEFOR2    %s\n", str(data2, size2).c_str());
  size_t new_size =
      protobuf_mutator::libfuzzer::CrossOverBinaryMessages<Document>(
          data1, size1, data2, size2, out, max_out_size, seed);
  fprintf(stderr, "AFTER     %s\n", str(data1, new_size).c_str());
  return new_size;
}

static Env* env = nullptr;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  // Environment has to be initialized in the same thread.
  if (env == nullptr)
    env = new Env();

  //  str(data, size);

  env->adapter->LoadHTML(str(data, size), "http://www.example.org");

  //  fprintf(stderr, "%s\n", writer.str().c_str());

  return 0;
}

}  // namespace content
