// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_HTML_FRAME_PROPERTIES_H_
#define COMPONENTS_HTML_VIEWER_HTML_FRAME_PROPERTIES_H_

#include "mojo/public/cpp/bindings/array.h"
#include "mojo/public/cpp/bindings/map.h"
#include "mojo/public/cpp/bindings/string.h"

namespace blink {
class WebFrame;
enum class WebSandboxFlags : int;
class WebString;
enum class WebTreeScopeType;
}

namespace url {
class Origin;
}

// Functions used to communicate html specific state for each frame.
namespace html_viewer {

// Keys used for client properties.
extern const char kPropertyFrameName[];
extern const char kPropertyFrameOrigin[];
extern const char kPropertyFrameSandboxFlags[];
extern const char kPropertyFrameTreeScope[];

mojo::Array<uint8_t> FrameNameToClientProperty(const blink::WebString& name);
blink::WebString FrameNameFromClientProperty(
    const mojo::Array<uint8_t>& new_data);

mojo::Array<uint8_t> FrameTreeScopeToClientProperty(
    blink::WebTreeScopeType scope_type);
bool FrameTreeScopeFromClientProperty(const mojo::Array<uint8_t>& new_data,
                                      blink::WebTreeScopeType* scope);

mojo::Array<uint8_t> FrameSandboxFlagsToClientProperty(
    blink::WebSandboxFlags flags);
bool FrameSandboxFlagsFromClientProperty(const mojo::Array<uint8_t>& new_data,
                                         blink::WebSandboxFlags* flags);

url::Origin FrameOrigin(blink::WebFrame* frame);
mojo::Array<uint8_t> FrameOriginToClientProperty(blink::WebFrame* frame);
url::Origin FrameOriginFromClientProperty(const mojo::Array<uint8_t>& data);

// Convenience to add |value| to |client_properties| if non-null.
void AddToClientPropertiesIfValid(
    const std::string& name,
    mojo::Array<uint8_t> value,
    mojo::Map<mojo::String, mojo::Array<uint8_t>>* client_properties);

// Returns |properties[name]| if exists, otherwise a null array.
mojo::Array<uint8_t> GetValueFromClientProperties(
    const std::string& name,
    const mojo::Map<mojo::String, mojo::Array<uint8_t>>& properties);

}  // namespace html_viewer

#endif  // COMPONENTS_HTML_VIEWER_HTML_FRAME_PROPERTIES_H_
