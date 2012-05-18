// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_RESOURCE_REQUEST_POLICY_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_RESOURCE_REQUEST_POLICY_H_
#pragma once

class ExtensionSet;
class GURL;
namespace WebKit {
class WebFrame;
}

// Encapsulates the policy for when chrome-extension:// URLs can be requested.
class ExtensionResourceRequestPolicy {
 public:
  // Returns true if the |resource_url| can be requested from |frame_url|.
  static bool CanRequestResource(const GURL& resource_url,
                                 WebKit::WebFrame* frame,
                                 const ExtensionSet* loaded_extensions);

 private:
  ExtensionResourceRequestPolicy();
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_RESOURCE_REQUEST_POLICY_H_
