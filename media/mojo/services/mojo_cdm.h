// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_keys.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "media/mojo/services/mojo_type_trait.h"

namespace mojo {
class ServiceProvider;
}

namespace media {

// A MediaKeys that proxies to a mojo::ContentDecryptionModule. That
// mojo::ContentDecryptionModule proxies back to the MojoCdm via the
// mojo::ContentDecryptionModuleClient interface.
class MojoCdm : public MediaKeys, public mojo::ContentDecryptionModuleClient {
 public:
  // |media_renderer_provider| is a ServiceProvider from a connected
  //     Application that is hosting a mojo::MediaRenderer.
  MojoCdm(mojo::ContentDecryptionModulePtr remote_cdm,
          const SessionMessageCB& session_message_cb,
          const SessionClosedCB& session_closed_cb,
          const SessionErrorCB& session_error_cb,
          const SessionKeysChangeCB& session_keys_change_cb,
          const SessionExpirationUpdateCB& session_expiration_update_cb);
  ~MojoCdm() final;

  // MediaKeys implementation.
  void SetServerCertificate(const uint8_t* certificate_data,
                            int certificate_data_length,
                            scoped_ptr<SimpleCdmPromise> promise) final;
  void CreateSessionAndGenerateRequest(
      SessionType session_type,
      const std::string& init_data_type,
      const uint8_t* init_data,
      int init_data_length,
      scoped_ptr<NewSessionCdmPromise> promise) final;
  void LoadSession(SessionType session_type,
                   const std::string& session_id,
                   scoped_ptr<NewSessionCdmPromise> promise) final;
  void UpdateSession(const std::string& session_id,
                     const uint8_t* response,
                     int response_length,
                     scoped_ptr<SimpleCdmPromise> promise) final;
  void CloseSession(const std::string& session_id,
                    scoped_ptr<SimpleCdmPromise> promise) final;
  void RemoveSession(const std::string& session_id,
                     scoped_ptr<SimpleCdmPromise> promise) final;
  CdmContext* GetCdmContext() final;

 private:
  // mojo::ContentDecryptionModuleClient implementation.
  void OnSessionMessage(const mojo::String& session_id,
                        mojo::CdmMessageType message_type,
                        mojo::Array<uint8_t> message,
                        const mojo::String& legacy_destination_url) final;
  void OnSessionClosed(const mojo::String& session_id) final;
  void OnSessionError(const mojo::String& session_id,
                      mojo::CdmException exception,
                      uint32_t system_code,
                      const mojo::String& error_message) final;
  void OnSessionKeysChange(
      const mojo::String& session_id,
      bool has_additional_usable_key,
      mojo::Array<mojo::CdmKeyInformationPtr> keys_info) final;
  void OnSessionExpirationUpdate(const mojo::String& session_id,
                                 int64_t new_expiry_time_usec) final;

  // Callbacks to handle CDM promises.
  // We have to inline this method, since MS VS 2013 compiler fails to compile
  // it when this method is not inlined. It fails with error C2244
  // "unable to match function definition to an existing declaration".
  template <typename... T>
  void OnPromiseResult(scoped_ptr<CdmPromiseTemplate<T...>> promise,
                       mojo::CdmPromiseResultPtr result,
                       typename MojoTypeTrait<T>::MojoType... args) {
    if (result->success)
      promise->resolve(args.template To<T>()...);  // See ISO C++03 14.2/4.
    else
      RejectPromise(promise.Pass(), result.Pass());
  }

  mojo::ContentDecryptionModulePtr remote_cdm_;
  mojo::Binding<ContentDecryptionModuleClient> binding_;

  // Callbacks for firing session events.
  SessionMessageCB session_message_cb_;
  SessionClosedCB session_closed_cb_;
  SessionErrorCB session_error_cb_;
  SessionKeysChangeCB session_keys_change_cb_;
  SessionExpirationUpdateCB session_expiration_update_cb_;

  base::WeakPtrFactory<MojoCdm> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdm);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_H_
