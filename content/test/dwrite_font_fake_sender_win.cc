// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/test/dwrite_font_fake_sender_win.h"

#include <dwrite.h>
#include <shlobj.h>

#include "content/common/dwrite_font_proxy_messages.h"

namespace content {

void AddFamily(const base::string16& font_path,
               const base::string16& family_name,
               const base::string16& base_family_name,
               FakeFontCollection* collection) {
  collection->AddFont(family_name)
      .AddFamilyName(L"en-us", family_name)
      .AddFilePath(font_path + L"\\" + base_family_name + L".ttf")
      .AddFilePath(font_path + L"\\" + base_family_name + L"bd.ttf")
      .AddFilePath(font_path + L"\\" + base_family_name + L"bi.ttf")
      .AddFilePath(font_path + L"\\" + base_family_name + L"i.ttf");
}

IPC::Sender* CreateFakeCollectionSender() {
  std::vector<base::char16> font_path_chars;
  font_path_chars.resize(MAX_PATH);
  SHGetSpecialFolderPath(nullptr /* hwndOwner - reserved */,
                         font_path_chars.data(), CSIDL_FONTS,
                         FALSE /* fCreate */);
  base::string16 font_path(font_path_chars.data());
  scoped_refptr<FakeFontCollection> fake_collection = new FakeFontCollection();
  AddFamily(font_path, L"Arial", L"arial", fake_collection.get());
  AddFamily(font_path, L"Courier New", L"cour", fake_collection.get());
  AddFamily(font_path, L"Times New Roman", L"times", fake_collection.get());
  return fake_collection->GetSender();
}

FakeFont::FakeFont(const base::string16& name) : font_name_(name) {}

FakeFont::FakeFont(const FakeFont& other) = default;

FakeFont::~FakeFont() = default;

FakeFontCollection::FakeFontCollection() = default;

FakeFont& FakeFontCollection::AddFont(const base::string16& font_name) {
  fonts_.emplace_back(font_name);
  return fonts_.back();
}

size_t FakeFontCollection::MessageCount() {
  return messages_.size();
}

IPC::Message* FakeFontCollection::GetMessage(size_t index) {
  return messages_[index].get();
}

IPC::Sender* FakeFontCollection::GetSender() {
  return new FakeSender(this, false /* track_messages */);
}

IPC::Sender* FakeFontCollection::GetTrackingSender() {
  return new FakeSender(this, true /* track_messages */);
}

FakeFontCollection::ReplySender::ReplySender(FakeFontCollection* collection)
    : collection_(collection) {}

FakeFontCollection::ReplySender::~ReplySender() = default;

std::unique_ptr<IPC::Message>&
FakeFontCollection::ReplySender::OnMessageReceived(const IPC::Message& msg) {
  reply_.reset();
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ReplySender, msg)
    IPC_MESSAGE_HANDLER(DWriteFontProxyMsg_FindFamily, OnFindFamily)
    IPC_MESSAGE_HANDLER(DWriteFontProxyMsg_GetFamilyCount, OnGetFamilyCount)
    IPC_MESSAGE_HANDLER(DWriteFontProxyMsg_GetFamilyNames, OnGetFamilyNames)
    IPC_MESSAGE_HANDLER(DWriteFontProxyMsg_GetFontFiles, OnGetFontFiles)
    IPC_MESSAGE_HANDLER(DWriteFontProxyMsg_MapCharacters, OnMapCharacters)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return reply_;
}

bool FakeFontCollection::ReplySender::Send(IPC::Message* msg) {
  reply_.reset(msg);
  return true;
}

void FakeFontCollection::ReplySender::OnFindFamily(
    const base::string16& family_name,
    uint32_t* index) {
  collection_->OnFindFamily(family_name, index);
}

void FakeFontCollection::ReplySender::OnGetFamilyCount(uint32_t* count) {
  collection_->OnGetFamilyCount(count);
}

void FakeFontCollection::ReplySender::OnGetFamilyNames(
    uint32_t family_index,
    std::vector<DWriteStringPair>* family_names) {
  collection_->OnGetFamilyNames(family_index, family_names);
}

void FakeFontCollection::ReplySender::OnGetFontFiles(
    uint32_t family_index,
    std::vector<base::string16>* file_paths) {
  collection_->OnGetFontFiles(family_index, file_paths);
}

void FakeFontCollection::ReplySender::OnMapCharacters(
    const base::string16& text,
    const DWriteFontStyle& font_style,
    const base::string16& locale_name,
    uint32_t reading_direction,
    const base::string16& base_family_name,
    MapCharactersResult* result) {
  collection_->OnMapCharacters(text, font_style, locale_name, reading_direction,
                               base_family_name, result);
}

FakeFontCollection::FakeSender::FakeSender(FakeFontCollection* collection,
                                           bool track_messages)
    : track_messages_(track_messages), collection_(collection) {}

FakeFontCollection::FakeSender::~FakeSender() = default;

bool FakeFontCollection::FakeSender::Send(IPC::Message* message) {
  std::unique_ptr<IPC::Message> incoming_message;
  if (track_messages_)
    collection_->messages_.emplace_back(message);
  else
    incoming_message.reset(message);  // Ensure message is deleted.
  std::unique_ptr<ReplySender> sender = collection_->GetReplySender();
  std::unique_ptr<IPC::Message> reply;
  reply.swap(sender->OnMessageReceived(*message));

  IPC::SyncMessage* sync_message = reinterpret_cast<IPC::SyncMessage*>(message);
  std::unique_ptr<IPC::MessageReplyDeserializer> serializer(
      sync_message->GetReplyDeserializer());
  serializer->SerializeOutputParameters(*(reply.get()));
  return true;
}

void FakeFontCollection::OnFindFamily(const base::string16& family_name,
                                      uint32_t* index) {
  for (size_t n = 0; n < fonts_.size(); n++) {
    if (_wcsicmp(family_name.data(), fonts_[n].font_name_.data()) == 0) {
      *index = n;
      return;
    }
  }
  *index = UINT32_MAX;
}

void FakeFontCollection::OnGetFamilyCount(uint32_t* count) {
  *count = fonts_.size();
}

void FakeFontCollection::OnGetFamilyNames(
    uint32_t family_index,
    std::vector<DWriteStringPair>* family_names) {
  if (family_index >= fonts_.size())
    return;
  *family_names = fonts_[family_index].family_names_;
}

void FakeFontCollection::OnGetFontFiles(
    uint32_t family_index,
    std::vector<base::string16>* file_paths) {
  if (family_index >= fonts_.size())
    return;
  *file_paths = fonts_[family_index].file_paths_;
}

void FakeFontCollection::OnMapCharacters(const base::string16& text,
                                         const DWriteFontStyle& font_style,
                                         const base::string16& locale_name,
                                         uint32_t reading_direction,
                                         const base::string16& base_family_name,
                                         MapCharactersResult* result) {
  result->family_index = 0;
  result->family_name = fonts_[0].font_name();
  result->mapped_length = 1;
  result->scale = 1.0;
  result->font_style.font_weight = DWRITE_FONT_WEIGHT_NORMAL;
  result->font_style.font_slant = DWRITE_FONT_STYLE_NORMAL;
  result->font_style.font_stretch = DWRITE_FONT_STRETCH_NORMAL;
}

std::unique_ptr<FakeFontCollection::ReplySender>
FakeFontCollection::GetReplySender() {
  return std::unique_ptr<FakeFontCollection::ReplySender>(
      new FakeFontCollection::ReplySender(this));
}

FakeFontCollection::~FakeFontCollection() = default;

}  // namespace content
