// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_PRESENTATION_SESSION_MESSAGE_H_
#define CONTENT_PUBLIC_BROWSER_PRESENTATION_SESSION_MESSAGE_H_

#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/common/content_export.h"

namespace content {

enum CONTENT_EXPORT PresentationMessageType {
  TEXT,
  ARRAY_BUFFER,
  BLOB,
};

// Represents a presentation session message.
// If this is a text message, |data| is null; otherwise, |message| is null.
// Empty messages are allowed.
struct CONTENT_EXPORT PresentationSessionMessage {
 public:
  ~PresentationSessionMessage();

  // Creates string message, which takes the ownership of |message|.
  static scoped_ptr<PresentationSessionMessage> CreateStringMessage(
      const std::string& presentation_url,
      const std::string& presentation_id,
      scoped_ptr<std::string> message);

  // Creates array buffer message, which takes the ownership of |data|.
  static scoped_ptr<PresentationSessionMessage> CreateArrayBufferMessage(
      const std::string& presentation_url,
      const std::string& presentation_id,
      scoped_ptr<std::vector<uint8_t>> data);

  // Creates blob message, which takes the ownership of |data|.
  static scoped_ptr<PresentationSessionMessage> CreateBlobMessage(
      const std::string& presentation_url,
      const std::string& presentation_id,
      scoped_ptr<std::vector<uint8_t>> data);

  bool is_binary() const;
  std::string presentation_url;
  std::string presentation_id;
  PresentationMessageType type;
  scoped_ptr<std::string> message;
  scoped_ptr<std::vector<uint8_t>> data;

 private:
  PresentationSessionMessage(const std::string& presentation_url,
                             const std::string& presentation_id,
                             scoped_ptr<std::string> message);
  PresentationSessionMessage(const std::string& presentation_url,
                             const std::string& presentation_id,
                             PresentationMessageType type,
                             scoped_ptr<std::vector<uint8_t>> data);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_PRESENTATION_SESSION_H_
