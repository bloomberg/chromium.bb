// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/html_viewer/html_frame_properties.h"

#include "base/logging.h"
#include "base/pickle.h"
#include "mojo/common/common_type_converters.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebSandboxFlags.h"
#include "url/origin.h"
#include "url/url_util.h"

namespace html_viewer {

const char kPropertyFrameTreeScope[] = "html-viewer-frame-tree-scope";
const char kPropertyFrameOrigin[] = "html-viewer-replicated-frame-origin";
const char kPropertyFrameName[] = "html-viewer-replicated-frame-name";
const char kPropertyFrameSandboxFlags[] = "html-viewer-sandbox-flags";

namespace {

mojo::Array<uint8_t> IntToClientPropertyArray(int value) {
  mojo::Array<uint8_t> value_array(sizeof(value));
  memcpy(&(value_array.front()), &value, sizeof(value));
  return value_array.Pass();
}

bool IntFromClientPropertyArray(const mojo::Array<uint8_t>& value_array,
                                int* value) {
  if (value_array.is_null())
    return false;

  CHECK(value_array.size() == sizeof(int));
  memcpy(value, &(value_array.front()), sizeof(int));
  return true;
}

}  // namespace

mojo::Array<uint8_t> FrameNameToClientProperty(const blink::WebString& name) {
  mojo::String mojo_name;
  if (name.isNull())
    return mojo::Array<uint8_t>();
  return mojo::Array<uint8_t>::From<std::string>(name.utf8());
}

blink::WebString FrameNameFromClientProperty(
    const mojo::Array<uint8_t>& new_data) {
  if (new_data.is_null())
    return blink::WebString();

  return blink::WebString::fromUTF8(new_data.To<std::string>());
}

mojo::Array<uint8_t> FrameTreeScopeToClientProperty(
    blink::WebTreeScopeType scope_type) {
  return IntToClientPropertyArray(static_cast<int>(scope_type)).Pass();
}

bool FrameTreeScopeFromClientProperty(const mojo::Array<uint8_t>& new_data,
                                      blink::WebTreeScopeType* scope) {
  if (new_data.is_null())
    return false;

  int scope_as_int;
  CHECK(IntFromClientPropertyArray(new_data, &scope_as_int));
  CHECK(scope_as_int >= 0 &&
        scope_as_int <= static_cast<int>(blink::WebTreeScopeType::Last));
  *scope = static_cast<blink::WebTreeScopeType>(scope_as_int);
  return true;
}

mojo::Array<uint8_t> FrameSandboxFlagsToClientProperty(
    blink::WebSandboxFlags flags) {
  return IntToClientPropertyArray(static_cast<int>(flags)).Pass();
}

bool FrameSandboxFlagsFromClientProperty(const mojo::Array<uint8_t>& new_data,
                                         blink::WebSandboxFlags* flags) {
  if (new_data.is_null())
    return false;

  // blink::WebSandboxFlags is a bitmask, so not error checking.
  int flags_as_int;
  CHECK(IntFromClientPropertyArray(new_data, &flags_as_int));
  *flags = static_cast<blink::WebSandboxFlags>(flags_as_int);
  return true;
}

url::Origin FrameOrigin(blink::WebFrame* frame) {
  std::string scheme = frame->document().securityOrigin().protocol().utf8();
  if (!url::IsStandard(scheme.c_str(),
                       url::Component(0, static_cast<int>(scheme.length())))) {
    return url::Origin();
  }
  return frame->document().securityOrigin();
}

mojo::Array<uint8_t> FrameOriginToClientProperty(blink::WebFrame* frame) {
  const url::Origin origin = FrameOrigin(frame);
  base::Pickle pickle;
  pickle.WriteBool(origin.unique());
  pickle.WriteString(origin.scheme());
  pickle.WriteString(origin.host());
  pickle.WriteUInt16(origin.port());
  mojo::Array<uint8_t> origin_array(pickle.size());
  memcpy(&(origin_array.front()), pickle.data(), pickle.size());
  return origin_array.Pass();
}

url::Origin FrameOriginFromClientProperty(const mojo::Array<uint8_t>& data) {
  if (data.is_null())
    return url::Origin();

  CHECK(data.size());
  CHECK(data.size() < static_cast<size_t>(std::numeric_limits<int>::max()));
  COMPILE_ASSERT(sizeof(uint8_t) == sizeof(unsigned char),
                 uint8_t_same_size_as_unsigned_char);
  const base::Pickle pickle(reinterpret_cast<const char*>(&(data.front())),
                            static_cast<int>(data.size()));
  CHECK(pickle.data());
  base::PickleIterator iter(pickle);
  bool unique = false;
  std::string scheme;
  std::string host;
  uint16_t port = 0;
  CHECK(iter.ReadBool(&unique));
  CHECK(iter.ReadString(&scheme));
  CHECK(iter.ReadString(&host));
  CHECK(iter.ReadUInt16(&port));
  const url::Origin result =
      unique ? url::Origin()
             : url::Origin::UnsafelyCreateOriginWithoutNormalization(
                   scheme, host, port);
  // If a unique origin was created, but the unique flag wasn't set, then
  // the values provided to 'UnsafelyCreateOriginWithoutNormalization' were
  // invalid; kill the renderer.
  CHECK(!(!unique && result.unique()));
  return result;
}

void AddToClientPropertiesIfValid(
    const std::string& name,
    mojo::Array<uint8_t> value,
    mojo::Map<mojo::String, mojo::Array<uint8_t>>* client_properties) {
  if (!value.is_null())
    (*client_properties)[name] = value.Pass();
}

mojo::Array<uint8_t> GetValueFromClientProperties(
    const std::string& name,
    const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties) {
  auto iter = properties.find(name);
  return iter == properties.end() ? mojo::Array<uint8_t>()
                                  : iter.GetValue().Clone().Pass();
}

}  // namespace html_viewer
