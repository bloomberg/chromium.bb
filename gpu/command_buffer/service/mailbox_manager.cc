// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager.h"

#include <algorithm>

#include "crypto/random.h"
#include "gpu/command_buffer/service/texture_manager.h"

namespace gpu {
namespace gles2 {

MailboxManager::MailboxManager()
    : mailbox_to_textures_(std::ptr_fun(&MailboxManager::TargetNameLess)) {
}

MailboxManager::~MailboxManager() {
  DCHECK(mailbox_to_textures_.empty());
  DCHECK(textures_to_mailboxes_.empty());
}

Texture* MailboxManager::ConsumeTexture(unsigned target,
                                        const Mailbox& mailbox) {
  MailboxToTextureMap::iterator it =
      mailbox_to_textures_.find(TargetName(target, mailbox));
  if (it == mailbox_to_textures_.end())
    return NULL;

  return it->second->first;
}

void MailboxManager::ProduceTexture(unsigned target,
                                    const Mailbox& mailbox,
                                    Texture* texture) {
  texture->SetMailboxManager(this);
  TargetName target_name(target, mailbox);
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

MailboxManager::TargetName::TargetName(unsigned target, const Mailbox& mailbox)
    : target(target),
      mailbox(mailbox) {
}

bool MailboxManager::TargetNameLess(const MailboxManager::TargetName& lhs,
                                    const MailboxManager::TargetName& rhs) {
  return memcmp(&lhs, &rhs, sizeof(lhs)) < 0;
}

}  // namespace gles2
}  // namespace gpu
