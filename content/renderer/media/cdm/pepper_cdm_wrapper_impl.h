// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_CDM_PEPPER_CDM_WRAPPER_IMPL_H_
#define CONTENT_RENDERER_MEDIA_CDM_PEPPER_CDM_WRAPPER_IMPL_H_

#include "media/media_features.h"

#if !BUILDFLAG(ENABLE_LIBRARY_CDMS)
#error This file should only be included when ENABLE_LIBRARY_CDMS is defined
#endif

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/renderer/media/cdm/pepper_cdm_wrapper.h"

namespace blink {
class WebHelperPlugin;
class WebLocalFrame;
}

namespace url {
class Origin;
}

namespace content {

class ContentDecryptorDelegate;
class PepperPluginInstanceImpl;

// Deleter for blink::WebHelperPlugin.
struct WebHelperPluginDeleter {
  void operator()(blink::WebHelperPlugin* plugin) const;
};

// Implements a wrapper on blink::WebHelperPlugin so that the plugin gets
// destroyed properly. It owns all the objects derived from WebHelperPlugin
// (WebPlugin, PepperPluginInstanceImpl, ContentDecryptionDelegate), and will
// free them as necessary when this wrapper is destroyed. In particular, it
// takes a reference to PepperPluginInstanceImpl so it won't go away until
// this object is destroyed.
//
// Implemented so that lower layers in Chromium don't need to be aware of
// blink:: objects.
class PepperCdmWrapperImpl : public PepperCdmWrapper {
 public:
  static std::unique_ptr<PepperCdmWrapper> Create(
      blink::WebLocalFrame* frame,
      const std::string& pluginType,
      const url::Origin& security_origin);

  ~PepperCdmWrapperImpl() override;

  // Returns the ContentDecryptorDelegate* associated with this plugin.
  ContentDecryptorDelegate* GetCdmDelegate() override;

 private:
  typedef std::unique_ptr<blink::WebHelperPlugin, WebHelperPluginDeleter>
      ScopedHelperPlugin;

  // Takes ownership of |helper_plugin| and |plugin_instance|.
  PepperCdmWrapperImpl(
      ScopedHelperPlugin helper_plugin,
      const scoped_refptr<PepperPluginInstanceImpl>& plugin_instance);

  ScopedHelperPlugin helper_plugin_;
  scoped_refptr<PepperPluginInstanceImpl> plugin_instance_;

  DISALLOW_COPY_AND_ASSIGN(PepperCdmWrapperImpl);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_CDM_PEPPER_CDM_WRAPPER_IMPL_H_
