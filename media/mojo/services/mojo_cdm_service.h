// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_
#define MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "media/base/media_keys.h"
#include "media/mojo/interfaces/content_decryption_module.mojom.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/binding.h"
#include "third_party/mojo/src/mojo/public/cpp/bindings/error_handler.h"

namespace media {

class MojoCdmServiceContext;

// A mojo::ContentDecryptionModule implementation backed by a media::MediaKeys.
class MojoCdmService : public mojo::ContentDecryptionModule,
                       public mojo::ErrorHandler {
 public:
  // Creates a MojoCdmService for |key_system| and weakly binds it to the
  // |request|. Returns null and drops the |request| if no MojoCdmService can be
  // created for |key_system|. When connection error happens,
  // ServiceHadConnectionError() will be called on the |context|.
  static scoped_ptr<MojoCdmService> Create(
      const mojo::String& key_system,
      MojoCdmServiceContext* context,
      mojo::InterfaceRequest<mojo::ContentDecryptionModule> request);

  ~MojoCdmService() final;

  // mojo::ContentDecryptionModule implementation.
  void SetClient(mojo::ContentDecryptionModuleClientPtr client) final;
  void SetServerCertificate(
      mojo::Array<uint8_t> certificate_data,
      const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) final;
  void CreateSessionAndGenerateRequest(
      mojo::ContentDecryptionModule::SessionType session_type,
      mojo::ContentDecryptionModule::InitDataType init_data_type,
      mojo::Array<uint8_t> init_data,
      const mojo::Callback<void(mojo::CdmPromiseResultPtr, mojo::String)>&
          callback) final;
  void LoadSession(mojo::ContentDecryptionModule::SessionType session_type,
                   const mojo::String& session_id,
                   const mojo::Callback<void(mojo::CdmPromiseResultPtr,
                                             mojo::String)>& callback) final;
  void UpdateSession(
      const mojo::String& session_id,
      mojo::Array<uint8_t> response,
      const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) final;
  void CloseSession(
      const mojo::String& session_id,
      const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) final;
  void RemoveSession(
      const mojo::String& session_id,
      const mojo::Callback<void(mojo::CdmPromiseResultPtr)>& callback) final;
  // TODO(xhwang): Rename this to GetDecryptor in the mojom interface.
  void GetCdmContext(int32_t cdm_id,
                     mojo::InterfaceRequest<mojo::Decryptor> decryptor) final;

  // Get CdmContext to be used by the media pipeline.
  CdmContext* GetCdmContext();

 private:
  MojoCdmService(const mojo::String& key_system,
                 MojoCdmServiceContext* context,
                 mojo::InterfaceRequest<mojo::ContentDecryptionModule> request);

  // mojo::ErrorHandler implementation.
  void OnConnectionError() override;

  // Callbacks for firing session events.
  void OnSessionMessage(const std::string& session_id,
                        MediaKeys::MessageType message_type,
                        const std::vector<uint8_t>& message,
                        const GURL& legacy_destination_url);
  void OnSessionKeysChange(const std::string& session_id,
                           bool has_additional_usable_key,
                           CdmKeysInfo keys_info);
  void OnSessionExpirationUpdate(const std::string& session_id,
                                 const base::Time& new_expiry_time);
  void OnSessionClosed(const std::string& session_id);
  void OnLegacySessionError(const std::string& session_id,
                            MediaKeys::Exception exception,
                            uint32_t system_code,
                            const std::string& error_message);

  mojo::Binding<mojo::ContentDecryptionModule> binding_;

  MojoCdmServiceContext* context_;
  scoped_ptr<MediaKeys> cdm_;

  mojo::ContentDecryptionModuleClientPtr client_;

  base::WeakPtr<MojoCdmService> weak_this_;
  base::WeakPtrFactory<MojoCdmService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MojoCdmService);
};

}  // namespace media

#endif  // MEDIA_MOJO_SERVICES_MOJO_CDM_SERVICE_H_
