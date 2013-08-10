// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/content_decryption_module_factory.h"

#include "base/logging.h"
#include "content/renderer/media/crypto/key_systems.h"
#include "media/cdm/aes_decryptor.h"

#if defined(ENABLE_PEPPER_CDMS)
#include "content/renderer/media/crypto/ppapi_decryptor.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/pepper_webplugin_impl.h"
#include "third_party/WebKit/public/platform/WebString.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "third_party/WebKit/public/web/WebMediaPlayerClient.h"
#elif defined(OS_ANDROID)
#include "content/renderer/media/android/proxy_media_keys.h"
#include "content/renderer/media/android/webmediaplayer_proxy_android.h"
#endif  // defined(ENABLE_PEPPER_CDMS)

namespace content {

#if defined(ENABLE_PEPPER_CDMS)
// Returns the PepperPluginInstanceImpl associated with the Helper Plugin.
// If a non-NULL pointer is returned, the caller must call closeHelperPlugin()
// when the Helper Plugin is no longer needed.
static scoped_refptr<PepperPluginInstanceImpl> CreateHelperPlugin(
    const std::string& plugin_type,
    WebKit::WebMediaPlayerClient* web_media_player_client,
    WebKit::WebFrame* web_frame) {
  DCHECK(web_media_player_client);
  DCHECK(web_frame);

  WebKit::WebPlugin* web_plugin = web_media_player_client->createHelperPlugin(
      WebKit::WebString::fromUTF8(plugin_type), web_frame);
  if (!web_plugin)
    return NULL;

  DCHECK(!web_plugin->isPlaceholder());  // Prevented by Blink.
  // Only Pepper plugins are supported, so it must be a ppapi object.
  PepperWebPluginImpl* ppapi_plugin =
      static_cast<PepperWebPluginImpl*>(web_plugin);
  return ppapi_plugin->instance();
}

static scoped_ptr<media::MediaKeys> CreatePpapiDecryptor(
    const std::string& key_system,
    const media::KeyAddedCB& key_added_cb,
    const media::KeyErrorCB& key_error_cb,
    const media::KeyMessageCB& key_message_cb,
    const base::Closure& destroy_plugin_cb,
    WebKit::WebMediaPlayerClient* web_media_player_client,
    WebKit::WebFrame* web_frame) {
  DCHECK(web_media_player_client);
  DCHECK(web_frame);

  std::string plugin_type = GetPepperType(key_system);
  DCHECK(!plugin_type.empty());
  const scoped_refptr<PepperPluginInstanceImpl>& plugin_instance =
      CreateHelperPlugin(plugin_type, web_media_player_client, web_frame);
  if (!plugin_instance.get()) {
    DLOG(ERROR) << "ProxyDecryptor: plugin instance creation failed.";
    return scoped_ptr<media::MediaKeys>();
  }

  scoped_ptr<PpapiDecryptor> decryptor =
      PpapiDecryptor::Create(key_system,
                             plugin_instance,
                             key_added_cb,
                             key_error_cb,
                             key_message_cb,
                             destroy_plugin_cb);

  if (!decryptor) {
    ContentDecryptionModuleFactory::DestroyHelperPlugin(
        web_media_player_client);
  }
  // Else the new object will call destroy_plugin_cb to destroy Helper Plugin.

  return scoped_ptr<media::MediaKeys>(decryptor.Pass());
}

void ContentDecryptionModuleFactory::DestroyHelperPlugin(
    WebKit::WebMediaPlayerClient* web_media_player_client) {
  web_media_player_client->closeHelperPlugin();
}
#endif  // defined(ENABLE_PEPPER_CDMS)

scoped_ptr<media::MediaKeys> ContentDecryptionModuleFactory::Create(
    const std::string& key_system,
#if defined(ENABLE_PEPPER_CDMS)
    WebKit::WebMediaPlayerClient* web_media_player_client,
    WebKit::WebFrame* web_frame,
    const base::Closure& destroy_plugin_cb,
#elif defined(OS_ANDROID)
    WebMediaPlayerProxyAndroid* proxy,
    int media_keys_id,
#endif  // defined(ENABLE_PEPPER_CDMS)
    const media::KeyAddedCB& key_added_cb,
    const media::KeyErrorCB& key_error_cb,
    const media::KeyMessageCB& key_message_cb) {
  if (CanUseAesDecryptor(key_system)) {
    return scoped_ptr<media::MediaKeys>(
        new media::AesDecryptor(key_added_cb, key_error_cb, key_message_cb));
  }

#if defined(ENABLE_PEPPER_CDMS)
  // TODO(ddorwin): Remove when the WD API implementation supports loading
  // Pepper-based CDMs: http://crbug.com/250049
  if (!web_media_player_client)
    return scoped_ptr<media::MediaKeys>();

  return CreatePpapiDecryptor(
      key_system, key_added_cb, key_error_cb, key_message_cb,
      destroy_plugin_cb, web_media_player_client, web_frame);
#elif defined(OS_ANDROID)
  scoped_ptr<ProxyMediaKeys> proxy_media_keys(
      new ProxyMediaKeys(proxy, media_keys_id));
  proxy_media_keys->InitializeCDM(key_system);
  return proxy_media_keys.PassAs<media::MediaKeys>();
#else
  return scoped_ptr<media::MediaKeys>();
#endif  // defined(ENABLE_PEPPER_CDMS)
}

}  // namespace content
