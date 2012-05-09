// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager.h"

#include "base/rand_util.h"
#include "crypto/hmac.h"
#include "gpu/command_buffer/service/gl_utils.h"
#include "gpu/command_buffer/service/texture_definition.h"

namespace gpu {
namespace gles2 {

MailboxManager::MailboxManager()
    : hmac_(crypto::HMAC::SHA256),
      textures_(std::ptr_fun(&MailboxManager::TargetNameLess)) {
  unsigned char private_key[GL_MAILBOX_SIZE_CHROMIUM / 2];
  base::RandBytes(private_key, sizeof(private_key));
  bool success = hmac_.Init(private_key, sizeof(private_key));
  DCHECK(success);
}

MailboxManager::~MailboxManager() {
}

void MailboxManager::GenerateMailboxName(MailboxName* name) {
  base::RandBytes(name->key, sizeof(name->key));
  SignMailboxName(name);
}

TextureDefinition* MailboxManager::ConsumeTexture(unsigned target,
                                                  const MailboxName& name) {
  if (!IsMailboxNameValid(name))
    return NULL;

  TextureDefinitionMap::iterator it =
      textures_.find(TargetName(target, name));
  if (it == textures_.end()) {
    NOTREACHED();
    return NULL;
  }

  TextureDefinition* definition = it->second.definition.release();
  textures_.erase(it);

  return definition;
}

bool MailboxManager::ProduceTexture(unsigned target,
                                    const MailboxName& name,
                                    TextureDefinition* definition,
                                    TextureManager* owner) {
  if (!IsMailboxNameValid(name))
    return false;

  TextureDefinitionMap::iterator it =
      textures_.find(TargetName(target, name));
  if (it != textures_.end()) {
    NOTREACHED();
    GLuint service_id = it->second.definition->ReleaseServiceId();
    glDeleteTextures(1, &service_id);
    it->second = OwnedTextureDefinition(definition, owner);
  } else {
    textures_.insert(std::make_pair(
        TargetName(target, name),
        OwnedTextureDefinition(definition, owner)));
  }

  return true;
}

void MailboxManager::DestroyOwnedTextures(TextureManager* owner,
                                          bool have_context) {
  TextureDefinitionMap::iterator it = textures_.begin();
  while (it != textures_.end()) {
    TextureDefinitionMap::iterator current_it = it;
    ++it;
    if (current_it->second.owner == owner) {
      NOTREACHED();
      GLuint service_id = current_it->second.definition->ReleaseServiceId();
      if (have_context)
        glDeleteTextures(1, &service_id);
      textures_.erase(current_it);
    }
  }
}

void MailboxManager::SignMailboxName(MailboxName* name) {
  bool success = hmac_.Sign(
      base::StringPiece(reinterpret_cast<char*>(name->key), sizeof(name->key)),
      reinterpret_cast<unsigned char*>(name->signature),
      sizeof(name->signature));
  DCHECK(success);
}

bool MailboxManager::IsMailboxNameValid(const MailboxName& name) {
  return hmac_.Verify(
      base::StringPiece(reinterpret_cast<const char*>(name.key),
          sizeof(name.key)),
      base::StringPiece(reinterpret_cast<const char*>(name.signature),
          sizeof(name.signature)));
}

MailboxManager::TargetName::TargetName(unsigned target, const MailboxName& name)
    : target(target),
      name(name) {
}

bool MailboxManager::TargetNameLess(const MailboxManager::TargetName& lhs,
                                    const MailboxManager::TargetName& rhs) {
  return memcmp(&lhs, &rhs, sizeof(lhs)) < 0;
}

MailboxManager::OwnedTextureDefinition::OwnedTextureDefinition(
    TextureDefinition* definition,
    TextureManager* owner)
    : definition(definition),
      owner(owner) {
}

MailboxManager::OwnedTextureDefinition::~OwnedTextureDefinition() {
}

}  // namespace gles2
}  // namespace gpu
