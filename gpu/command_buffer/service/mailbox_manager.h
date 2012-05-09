// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_
#define GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_

#include <functional>
#include <map>

#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "crypto/hmac.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/gpu_export.h"

// From gl2/gl2ext.h.
#ifndef GL_MAILBOX_SIZE_CHROMIUM
#define GL_MAILBOX_SIZE_CHROMIUM 64
#endif

typedef signed char GLbyte;

namespace gpu {
namespace gles2 {

class TextureDefinition;
class TextureManager;

// Identifies a mailbox where a texture definition can be stored for
// transferring textures between contexts that are not in the same context
// group. It is a random key signed with a hash of a private key.
struct MailboxName {
  GLbyte key[GL_MAILBOX_SIZE_CHROMIUM / 2];
  GLbyte signature[GL_MAILBOX_SIZE_CHROMIUM / 2];
};

// Manages resources scoped beyond the context or context group level.
class GPU_EXPORT MailboxManager : public base::RefCounted<MailboxManager> {
 public:
  MailboxManager();

  // Generate a unique mailbox name signed with the manager's private key.
  void GenerateMailboxName(MailboxName* name);

  // Remove the texture definition from the named mailbox and empty the mailbox.
  TextureDefinition* ConsumeTexture(unsigned target, const MailboxName& name);

  // Put the texture definition in the named mailbox.
  bool ProduceTexture(unsigned target,
                      const MailboxName& name,
                      TextureDefinition* definition,
                      TextureManager* owner);

  // Destroy any texture definitions and mailboxes owned by the given texture
  // manager.
  void DestroyOwnedTextures(TextureManager* owner, bool have_context);

 private:
  friend class base::RefCounted<MailboxManager>;

  ~MailboxManager();

  void SignMailboxName(MailboxName* name);
  bool IsMailboxNameValid(const MailboxName& name);

  struct TargetName {
    TargetName(unsigned target, const MailboxName& name);
    unsigned target;
    MailboxName name;
  };

  static bool TargetNameLess(const TargetName& lhs, const TargetName& rhs);

  struct OwnedTextureDefinition {
    OwnedTextureDefinition(TextureDefinition* definition,
                           TextureManager* owner);
    ~OwnedTextureDefinition();
    linked_ptr<TextureDefinition> definition;
    TextureManager* owner;
  };

  typedef std::map<
      TargetName,
      OwnedTextureDefinition,
      std::pointer_to_binary_function<
          const TargetName&, const TargetName&, bool> > TextureDefinitionMap;

  crypto::HMAC hmac_;
  TextureDefinitionMap textures_;

  DISALLOW_COPY_AND_ASSIGN(MailboxManager);
};
}  // namespage gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_MAILBOX_MANAGER_H_


