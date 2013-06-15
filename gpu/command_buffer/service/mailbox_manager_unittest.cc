// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/mailbox_manager.h"

#include "gpu/command_buffer/service/texture_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {
namespace gles2 {

class MailboxManagerTest : public testing::Test {
 public:
  MailboxManagerTest() : manager_(new MailboxManager()) {}
  virtual ~MailboxManagerTest() {}

 protected:
  Texture* CreateTexture() {
    return new Texture(0);
  }

  void DestroyTexture(Texture* texture) {
    delete texture;
  }

  scoped_refptr<MailboxManager> manager_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MailboxManagerTest);
};

// Tests basic produce/consume behavior.
TEST_F(MailboxManagerTest, Basic) {
  Texture* texture = CreateTexture();

  MailboxName name;
  manager_->GenerateMailboxName(&name);
  EXPECT_TRUE(manager_->ProduceTexture(0, name, texture));
  EXPECT_EQ(texture, manager_->ConsumeTexture(0, name));

  // We can consume multiple times.
  EXPECT_EQ(texture, manager_->ConsumeTexture(0, name));

  // Wrong target should fail the consume.
  EXPECT_EQ(NULL, manager_->ConsumeTexture(1, name));

  // Destroy should cleanup the mailbox.
  DestroyTexture(texture);
  EXPECT_EQ(NULL, manager_->ConsumeTexture(0, name));
}

// Should fail to produce or consume with an invalid mailbox.
TEST_F(MailboxManagerTest, InvalidName) {
  Texture* texture = CreateTexture();
  MailboxName name;
  memset(&name, 0, sizeof(name));
  EXPECT_FALSE(manager_->ProduceTexture(0, name, texture));
  EXPECT_EQ(NULL, manager_->ConsumeTexture(0, name));
  DestroyTexture(texture);
}

// Tests behavior with multiple produce on the same texture.
TEST_F(MailboxManagerTest, ProduceMultipleMailbox) {
  Texture* texture = CreateTexture();

  MailboxName name1;
  manager_->GenerateMailboxName(&name1);

  EXPECT_TRUE(manager_->ProduceTexture(0, name1, texture));
  EXPECT_EQ(texture, manager_->ConsumeTexture(0, name1));

  // Can produce a second time with the same mailbox.
  EXPECT_TRUE(manager_->ProduceTexture(0, name1, texture));
  EXPECT_EQ(texture, manager_->ConsumeTexture(0, name1));

  // Can produce again, with a different mailbox.
  MailboxName name2;
  manager_->GenerateMailboxName(&name2);
  EXPECT_TRUE(manager_->ProduceTexture(0, name2, texture));

  // Still available under all mailboxes.
  EXPECT_EQ(texture, manager_->ConsumeTexture(0, name1));
  EXPECT_EQ(texture, manager_->ConsumeTexture(0, name2));

  // Destroy should cleanup all mailboxes.
  DestroyTexture(texture);
  EXPECT_EQ(NULL, manager_->ConsumeTexture(0, name1));
  EXPECT_EQ(NULL, manager_->ConsumeTexture(0, name2));
}

// Tests behavior with multiple produce on the same mailbox with different
// textures.
TEST_F(MailboxManagerTest, ProduceMultipleTexture) {
  Texture* texture1 = CreateTexture();
  Texture* texture2 = CreateTexture();

  MailboxName name;
  manager_->GenerateMailboxName(&name);

  EXPECT_TRUE(manager_->ProduceTexture(0, name, texture1));
  EXPECT_EQ(texture1, manager_->ConsumeTexture(0, name));

  // Can produce a second time with the same mailbox, but different texture.
  EXPECT_TRUE(manager_->ProduceTexture(0, name, texture2));
  EXPECT_EQ(texture2, manager_->ConsumeTexture(0, name));

  // Destroying the texture that's under no mailbox shouldn't have an effect.
  DestroyTexture(texture1);
  EXPECT_EQ(texture2, manager_->ConsumeTexture(0, name));

  // Destroying the texture that's bound should clean up.
  DestroyTexture(texture2);
  EXPECT_EQ(NULL, manager_->ConsumeTexture(0, name));
}

TEST_F(MailboxManagerTest, ProduceMultipleTextureMailbox) {
  Texture* texture1 = CreateTexture();
  Texture* texture2 = CreateTexture();
  MailboxName name1;
  manager_->GenerateMailboxName(&name1);
  MailboxName name2;
  manager_->GenerateMailboxName(&name2);

  // Put texture1 on name1 and name2.
  EXPECT_TRUE(manager_->ProduceTexture(0, name1, texture1));
  EXPECT_TRUE(manager_->ProduceTexture(0, name2, texture1));
  EXPECT_EQ(texture1, manager_->ConsumeTexture(0, name1));
  EXPECT_EQ(texture1, manager_->ConsumeTexture(0, name2));

  // Put texture2 on name2.
  EXPECT_TRUE(manager_->ProduceTexture(0, name2, texture2));
  EXPECT_EQ(texture1, manager_->ConsumeTexture(0, name1));
  EXPECT_EQ(texture2, manager_->ConsumeTexture(0, name2));

  // Destroy texture1, shouldn't affect name2.
  DestroyTexture(texture1);
  EXPECT_EQ(NULL, manager_->ConsumeTexture(0, name1));
  EXPECT_EQ(texture2, manager_->ConsumeTexture(0, name2));

  DestroyTexture(texture2);
  EXPECT_EQ(NULL, manager_->ConsumeTexture(0, name2));
}

}  // namespace gles2
}  // namespace gpu
