// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_TEST_DWRITE_FONT_FAKE_SENDER_WIN_H_
#define CONTENT_TEST_DWRITE_FONT_FAKE_SENDER_WIN_H_

#include <stddef.h>
#include <stdint.h>
#include <wrl.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"

struct DWriteFontStyle;
struct MapCharactersResult;

namespace content {

class FakeFontCollection;

// Creates a new FakeFontCollection, seeded with some basic data, and returns a
// Sender that can be used to interact with the collection.
IPC::Sender* CreateFakeCollectionSender();

// Helper class for describing a font object. Use FakeFontCollection instead.
class FakeFont {
 public:
  explicit FakeFont(const base::string16& name);

  FakeFont(const FakeFont& other);

  ~FakeFont();

  FakeFont& AddFilePath(const base::string16& file_path) {
    file_paths_.push_back(file_path);
    return *this;
  }

  FakeFont& AddFamilyName(const base::string16& locale,
                          const base::string16& family_name) {
    family_names_.emplace_back(locale, family_name);
    return *this;
  }

  const base::string16& font_name() { return font_name_; }

 private:
  friend FakeFontCollection;
  base::string16 font_name_;
  std::vector<base::string16> file_paths_;
  std::vector<std::pair<base::string16, base::string16>> family_names_;

  DISALLOW_ASSIGN(FakeFont);
};

// Implements a font collection that supports interaction through sending IPC
// messages from dwrite_font_proxy_messages.h.
// Usage:
//   Create a new FakeFontCollection.
//   Call AddFont() to add fonts.
//     For each font, call methods on the font to configure the font.
//     Note: the indices of the fonts will correspond to the order they were
//         added. The collection will not sort or reorder fonts in any way.
//   Call GetSender()/GetTrackingSender() to obtain an IPC::Sender.
//   Call Send() with DWriteFontProxyMsg_* to interact with the collection.
//   Call MessageCount()/GetMessage() to inspect the messages that were sent.
//
// The internal code flow for GetSender()/Send() is as follows:
//   GetSender() returns a new FakeSender, pointing back to the collection.
//   FakeSender::Send() will create a new ReplySender and call
//       ReplySender::OnMessageReceived()
//   ReplySender::OnMessageReceived() contains the message map, which will
//       internally call ReplySender::On*() and ReplySender::Send()
//   ReplySender::Send() will save the reply message, to be used later.
//   FakeSender::Send() will retrieve the reply message and save the output.
class FakeFontCollection : public base::RefCounted<FakeFontCollection> {
 public:
  FakeFontCollection();

  FakeFont& AddFont(const base::string16& font_name);

  size_t MessageCount();

  IPC::Message* GetMessage(size_t index);

  IPC::Sender* GetSender();

  // Like GetSender(), but will keep track of all sent messages for inspection.
  IPC::Sender* GetTrackingSender();

 protected:
  // This class handles sending the reply message back to the "renderer"
  class ReplySender : public IPC::Sender {
   public:
    explicit ReplySender(FakeFontCollection* collection);

    ~ReplySender() override;

    std::unique_ptr<IPC::Message>& OnMessageReceived(const IPC::Message& msg);

    bool Send(IPC::Message* msg) override;

   private:
    void OnFindFamily(const base::string16& family_name, uint32_t* index);

    void OnGetFamilyCount(uint32_t* count);

    void OnGetFamilyNames(
        uint32_t family_index,
        std::vector<std::pair<base::string16, base::string16>>* family_names);
    void OnGetFontFiles(uint32_t family_index,
                        std::vector<base::string16>* file_paths_);

    void OnMapCharacters(const base::string16& text,
                         const DWriteFontStyle& font_style,
                         const base::string16& locale_name,
                         uint32_t reading_direction,
                         const base::string16& base_family_name,
                         MapCharactersResult* result);

   private:
    scoped_refptr<FakeFontCollection> collection_;
    std::unique_ptr<IPC::Message> reply_;

    DISALLOW_COPY_AND_ASSIGN(ReplySender);
  };

  // This class can be used by the "renderer" to send messages to the "browser"
  class FakeSender : public IPC::Sender {
   public:
    FakeSender(FakeFontCollection* collection, bool track_messages);

    ~FakeSender() override;

    bool Send(IPC::Message* message) override;

   private:
    bool track_messages_;
    scoped_refptr<FakeFontCollection> collection_;

    DISALLOW_COPY_AND_ASSIGN(FakeSender);
  };

  void OnFindFamily(const base::string16& family_name, uint32_t* index);

  void OnGetFamilyCount(uint32_t* count);

  void OnGetFamilyNames(
      uint32_t family_index,
      std::vector<std::pair<base::string16, base::string16>>* family_names);

  void OnGetFontFiles(uint32_t family_index,
                      std::vector<base::string16>* file_paths);

  void OnMapCharacters(const base::string16& text,
                       const DWriteFontStyle& font_style,
                       const base::string16& locale_name,
                       uint32_t reading_direction,
                       const base::string16& base_family_name,
                       MapCharactersResult* result);

  std::unique_ptr<ReplySender> GetReplySender();

 private:
  friend class base::RefCounted<FakeFontCollection>;
  ~FakeFontCollection();

  std::vector<FakeFont> fonts_;
  std::vector<std::unique_ptr<IPC::Message>> messages_;

  DISALLOW_COPY_AND_ASSIGN(FakeFontCollection);
};

}  // namespace content

#endif  // CONTENT_TEST_DWRITE_FONT_FAKE_SENDER_WIN_H_
