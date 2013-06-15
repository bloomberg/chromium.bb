// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager.h"

#include <algorithm>

#include "base/rand_util.h"
#include "crypto/hmac.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {
namespace gles2 {

MailboxName::MailboxName() {
  std::fill(key, key + sizeof(key), 0);
  std::fill(signature, signature + sizeof(signature), 0);
}

MailboxManager::MailboxManager()
    : hmac_(crypto::HMAC::SHA256),
      mailbox_to_textures_(std::ptr_fun(&MailboxManager::TargetNameLess)) {
  base::RandBytes(private_key_, sizeof(private_key_));
  bool success = hmac_.Init(
      base::StringPiece(private_key_, sizeof(private_key_)));
  DCHECK(success);
  DCHECK(!IsMailboxNameValid(MailboxName()));
}

MailboxManager::~MailboxManager() {
  DCHECK(mailbox_to_textures_.empty());
  DCHECK(textures_to_mailboxes_.empty());
}

void MailboxManager::GenerateMailboxName(MailboxName* name) {
  base::RandBytes(name->key, sizeof(name->key));
  SignMailboxName(name);
}

Texture* MailboxManager::ConsumeTexture(unsigned target,
                                        const MailboxName& name) {
  if (!IsMailboxNameValid(name))
    return NULL;

  MailboxToTextureMap::iterator it =
      mailbox_to_textures_.find(TargetName(target, name));
  if (it == mailbox_to_textures_.end())
    return NULL;

  return it->second->first;
}

bool MailboxManager::ProduceTexture(unsigned target,
                                    const MailboxName& name,
                                    Texture* texture) {
  if (!IsMailboxNameValid(name))
    return false;

  texture->SetMailboxManager(this);
  TargetName target_name(target, name);
  MailboxToTextureMap::iterator it = mailbox_to_textures_.find(target_name);
  if (it != mailbox_to_textures_.end()) {
    TextureToMailboxMap::iterator texture_it = it->second;
    mailbox_to_textures_.erase(it);
    textures_to_mailboxes_.erase(texture_it);
  }
  TextureToMailboxMap::iterator texture_it =
      textures_to_mailboxes_.insert(std::make_pair(texture, target_name));
  mailbox_to_textures_.insert(std::make_pair(target_name, texture_it));
  DCHECK_EQ(mailbox_to_textures_.size(), textures_to_mailboxes_.size());

  return true;
}

void MailboxManager::TextureDeleted(Texture* texture) {
  std::pair<TextureToMailboxMap::iterator,
            TextureToMailboxMap::iterator> range =
      textures_to_mailboxes_.equal_range(texture);
  for (TextureToMailboxMap::iterator it = range.first;
       it != range.second; ++it) {
    size_t count = mailbox_to_textures_.erase(it->second);
    DCHECK(count == 1);
  }
  textures_to_mailboxes_.erase(range.first, range.second);
  DCHECK_EQ(mailbox_to_textures_.size(), textures_to_mailboxes_.size());
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

}  // namespace gles2
}  // namespace gpu
