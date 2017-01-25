// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_REMOTING_REMOTING_CDM_CONTEXT_H_
#define MEDIA_REMOTING_REMOTING_CDM_CONTEXT_H_

#include "media/base/cdm_context.h"

namespace media {
namespace remoting {

class RemotingCdm;
class SharedSession;

// TODO(xjz): Merge this with erickung's implementation.
// TODO(miu): This class should just be merged into RemotingCdm and implement
// both the CDM and CdmContext interfaces. Also, replace the GetSharedSession()
// accessor and move it to a new SharedSession::FromCdmContext() function. Then,
// neither the controller nor renderer can gain direct access to the CDM impl.
// See discussion in https://codereview.chromium.org/2643253003 for more info.
class RemotingCdmContext : public CdmContext {
 public:
  explicit RemotingCdmContext(RemotingCdm* remoting_cdm);
  ~RemotingCdmContext() override;

  // If |cdm_context| is an instance of RemotingCdmContext, return a type-casted
  // pointer to it. Otherwise, return nullptr.
  static RemotingCdmContext* From(CdmContext* cdm_context);

  SharedSession* GetSharedSession() const;

  // CdmContext implementations.
  Decryptor* GetDecryptor() override;
  int GetCdmId() const override;
  void* GetClassIdentifier() const override;

 private:
  RemotingCdm* const remoting_cdm_;  // Outlives this class.

  DISALLOW_COPY_AND_ASSIGN(RemotingCdmContext);
};

}  // namespace remoting
}  // namespace media

#endif  // MEDIA_REMOTING_REMOTING_CDM_CONTEXT_H_
