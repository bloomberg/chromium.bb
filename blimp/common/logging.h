// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_COMMON_LOGGING_H_
#define BLIMP_COMMON_LOGGING_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/macros.h"
#include "blimp/common/blimp_common_export.h"
#include "blimp/common/proto/blimp_message.pb.h"

namespace blimp {

class BlimpMessage;
class LogExtractor;

typedef std::vector<std::pair<std::string, std::string>> LogFields;

// Defines an interface for classes that extract a set of loggable field
// values from a message.
class BLIMP_COMMON_EXPORT LogExtractor {
 public:
  virtual ~LogExtractor() {}

  // |message|: The message which is used for field extraction.
  // |output|: A vector of KV pairs which will receive the extracted fields.
  virtual void ExtractFields(const BlimpMessage& message,
                             LogFields* output) const = 0;
};

// Registry of log extractors.
class BLIMP_COMMON_EXPORT BlimpMessageLogger {
 public:
  BlimpMessageLogger();
  ~BlimpMessageLogger();

  // Formats the loggable representation of |message| and sends it to |out|.
  void LogMessageToStream(const BlimpMessage& message, std::ostream* out) const;

 private:
  // Adds |extractor| to the registry for parsing messages of type |type|.
  // |type_name|: The human readable name of |type|.
  void AddHandler(const std::string& type_name,
                  BlimpMessage::Type type,
                  std::unique_ptr<LogExtractor> extractor);

  // Registry of log extractors. Map structure is:
  // {message type => (human readable message type, LogExtractor*)}
  std::map<BlimpMessage::Type,
           std::pair<std::string, std::unique_ptr<LogExtractor>>>
      extractors_;

  DISALLOW_COPY_AND_ASSIGN(BlimpMessageLogger);
};

// Serializes a BlimpMessage in a human-readable format for logging.
// An example message would look like:
// "<type=TAB_CONTROL subtype=SIZE size=640x480:1.000000 size=19>"
BLIMP_COMMON_EXPORT std::ostream& operator<<(std::ostream& out,
                                             const BlimpMessage& message);

}  // namespace blimp

#endif  // BLIMP_COMMON_LOGGING_H_
