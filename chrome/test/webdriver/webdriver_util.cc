// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/webdriver/webdriver_util.h"

#include "base/base64.h"
#include "base/basictypes.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/format_macros.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/third_party/icu/icu_utf.h"
#include "chrome/common/automation_id.h"
#include "chrome/common/zip.h"
#include "chrome/test/automation/automation_json_requests.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace webdriver {

SkipParsing* kSkipParsing = NULL;

std::string GenerateRandomID() {
  uint64 msb = base::RandUint64();
  uint64 lsb = base::RandUint64();
  return base::StringPrintf("%016" PRIx64 "%016" PRIx64, msb, lsb);
}

bool Base64Decode(const std::string& base64,
                  std::string* bytes) {
  std::string copy = base64;
  // Some WebDriver client base64 encoders follow RFC 1521, which require that
  // 'encoded lines be no more than 76 characters long'. Just remove any
  // newlines.
  RemoveChars(copy, "\n", &copy);
  return base::Base64Decode(copy, bytes);
}

namespace {

bool UnzipArchive(const base::FilePath& unzip_dir,
                  const std::string& bytes,
                  std::string* error_msg) {
  base::ScopedTempDir dir;
  if (!dir.CreateUniqueTempDir()) {
    *error_msg = "Unable to create temp dir";
    return false;
  }
  base::FilePath archive = dir.path().AppendASCII("temp.zip");
  int length = bytes.length();
  if (file_util::WriteFile(archive, bytes.c_str(), length) != length) {
    *error_msg = "Could not write file to temp dir";
    return false;
  }
  if (!zip::Unzip(archive, unzip_dir)) {
    *error_msg = "Could not unzip archive";
    return false;
  }
  return true;
}

}  // namespace

bool Base64DecodeAndUnzip(const base::FilePath& unzip_dir,
                          const std::string& base64,
                          std::string* error_msg) {
  std::string zip_data;
  if (!Base64Decode(base64, &zip_data)) {
    *error_msg = "Could not decode base64 zip data";
    return false;
  }
  return UnzipArchive(unzip_dir, zip_data, error_msg);
}

namespace {

// Stream for writing binary data.
class DataOutputStream {
 public:
  DataOutputStream() {}
  ~DataOutputStream() {}

  void WriteUInt16(uint16 data) {
    WriteBytes(&data, sizeof(data));
  }

  void WriteUInt32(uint32 data) {
    WriteBytes(&data, sizeof(data));
  }

  void WriteString(const std::string& data) {
    WriteBytes(data.c_str(), data.length());
  }

  void WriteBytes(const void* bytes, int size) {
    size_t next = buffer_.length();
    buffer_.resize(next + size);
    memcpy(&buffer_[next], bytes, size);
  }

  const std::string& buffer() const { return buffer_; }

 private:
  std::string buffer_;
};

// Stream for reading binary data.
class DataInputStream {
 public:
  DataInputStream(const char* data, int size)
      : data_(data), size_(size), iter_(0) {}
  ~DataInputStream() {}

  bool ReadUInt16(uint16* data) {
    return ReadBytes(data, sizeof(*data));
  }

  bool ReadUInt32(uint32* data) {
    return ReadBytes(data, sizeof(*data));
  }

  bool ReadString(std::string* data, int length) {
    if (length < 0)
      return false;
    // Check here to make sure we don't allocate wastefully.
    if (iter_ + length > size_)
      return false;
    data->resize(length);
    return ReadBytes(&(*data)[0], length);
  }

  bool ReadBytes(void* bytes, int size) {
    if (iter_ + size > size_)
      return false;
    memcpy(bytes, &data_[iter_], size);
    iter_ += size;
    return true;
  }

  int remaining() const { return size_ - iter_; }

 private:
  const char* data_;
  int size_;
  int iter_;
};

// A file entry within a zip archive. This may be incomplete and is not
// guaranteed to be able to parse all types of zip entries.
// See http://www.pkware.com/documents/casestudies/APPNOTE.TXT for the zip
// file format.
struct ZipEntry {
  // The given bytes must contain the whole zip entry and only the entry,
  // although the entry may include a data descriptor.
  static bool FromBytes(const std::string& bytes, ZipEntry* zip,
                        std::string* error_msg) {
    DataInputStream stream(bytes.c_str(), bytes.length());

    uint32 signature;
    if (!stream.ReadUInt32(&signature) || signature != kFileHeaderSignature) {
      *error_msg = "Invalid file header signature";
      return false;
    }
    if (!stream.ReadUInt16(&zip->version_needed)) {
      *error_msg = "Invalid version";
      return false;
    }
    if (!stream.ReadUInt16(&zip->bit_flag)) {
      *error_msg = "Invalid bit flag";
      return false;
    }
    if (!stream.ReadUInt16(&zip->compression_method)) {
      *error_msg = "Invalid compression method";
      return false;
    }
    if (!stream.ReadUInt16(&zip->mod_time)) {
      *error_msg = "Invalid file last modified time";
      return false;
    }
    if (!stream.ReadUInt16(&zip->mod_date)) {
      *error_msg = "Invalid file last modified date";
      return false;
    }
    if (!stream.ReadUInt32(&zip->crc)) {
      *error_msg = "Invalid crc";
      return false;
    }
    uint32 compressed_size;
    if (!stream.ReadUInt32(&compressed_size)) {
      *error_msg = "Invalid compressed size";
      return false;
    }
    if (!stream.ReadUInt32(&zip->uncompressed_size)) {
      *error_msg = "Invalid compressed size";
      return false;
    }
    uint16 name_length;
    if (!stream.ReadUInt16(&name_length)) {
      *error_msg = "Invalid name length";
      return false;
    }
    uint16 field_length;
    if (!stream.ReadUInt16(&field_length)) {
      *error_msg = "Invalid field length";
      return false;
    }
    if (!stream.ReadString(&zip->name, name_length)) {
      *error_msg = "Invalid name";
      return false;
    }
    if (!stream.ReadString(&zip->fields, field_length)) {
      *error_msg = "Invalid fields";
      return false;
    }
    if (zip->bit_flag & 0x8) {
      // Has compressed data and a separate data descriptor.
      if (stream.remaining() < 16) {
        *error_msg = "Too small for data descriptor";
        return false;
      }
      compressed_size = stream.remaining() - 16;
      if (!stream.ReadString(&zip->compressed_data, compressed_size)) {
        *error_msg = "Invalid compressed data before descriptor";
        return false;
      }
      if (!stream.ReadUInt32(&signature) ||
          signature != kDataDescriptorSignature) {
        *error_msg = "Invalid data descriptor signature";
        return false;
      }
      if (!stream.ReadUInt32(&zip->crc)) {
        *error_msg = "Invalid crc";
        return false;
      }
      if (!stream.ReadUInt32(&compressed_size)) {
        *error_msg = "Invalid compressed size";
        return false;
      }
      if (compressed_size != zip->compressed_data.length()) {
        *error_msg = "Compressed data does not match data descriptor";
        return false;
      }
      if (!stream.ReadUInt32(&zip->uncompressed_size)) {
        *error_msg = "Invalid compressed size";
        return false;
      }
    } else {
      // Just has compressed data.
      if (!stream.ReadString(&zip->compressed_data, compressed_size)) {
        *error_msg = "Invalid compressed data";
        return false;
      }
      if (stream.remaining() != 0) {
        *error_msg = "Leftover data after zip entry";
        return false;
      }
    }
    return true;
  }

  // Returns bytes for a valid zip file that just contains this zip entry.
  std::string ToZip() {
    // Write zip entry with no data descriptor.
    DataOutputStream stream;
    stream.WriteUInt32(kFileHeaderSignature);
    stream.WriteUInt16(version_needed);
    stream.WriteUInt16(bit_flag);
    stream.WriteUInt16(compression_method);
    stream.WriteUInt16(mod_time);
    stream.WriteUInt16(mod_date);
    stream.WriteUInt32(crc);
    stream.WriteUInt32(compressed_data.length());
    stream.WriteUInt32(uncompressed_size);
    stream.WriteUInt16(name.length());
    stream.WriteUInt16(fields.length());
    stream.WriteString(name);
    stream.WriteString(fields);
    stream.WriteString(compressed_data);
    uint32 entry_size = stream.buffer().length();

    // Write central directory.
    stream.WriteUInt32(kCentralDirSignature);
    stream.WriteUInt16(0x14);  // Version made by. Unused at version 0.
    stream.WriteUInt16(version_needed);
    stream.WriteUInt16(bit_flag);
    stream.WriteUInt16(compression_method);
    stream.WriteUInt16(mod_time);
    stream.WriteUInt16(mod_date);
    stream.WriteUInt32(crc);
    stream.WriteUInt32(compressed_data.length());
    stream.WriteUInt32(uncompressed_size);
    stream.WriteUInt16(name.length());
    stream.WriteUInt16(fields.length());
    stream.WriteUInt16(0);  // Comment length.
    stream.WriteUInt16(0);  // Disk number where file starts.
    stream.WriteUInt16(0);  // Internal file attr.
    stream.WriteUInt32(0);  // External file attr.
    stream.WriteUInt32(0);  // Offset to file.
    stream.WriteString(name);
    stream.WriteString(fields);
    uint32 cd_size = stream.buffer().length() - entry_size;

    // End of central directory.
    stream.WriteUInt32(kEndOfCentralDirSignature);
    stream.WriteUInt16(0);  // num of this disk
    stream.WriteUInt16(0);  // disk where cd starts
    stream.WriteUInt16(1);  // number of cds on this disk
    stream.WriteUInt16(1);  // total cds
    stream.WriteUInt32(cd_size);  // size of cd
    stream.WriteUInt32(entry_size);  // offset of cd
    stream.WriteUInt16(0);  // comment len

    return stream.buffer();
  }

  static const uint32 kFileHeaderSignature;
  static const uint32 kDataDescriptorSignature;
  static const uint32 kCentralDirSignature;
  static const uint32 kEndOfCentralDirSignature;
  uint16 version_needed;
  uint16 bit_flag;
  uint16 compression_method;
  uint16 mod_time;
  uint16 mod_date;
  uint32 crc;
  uint32 uncompressed_size;
  std::string name;
  std::string fields;
  std::string compressed_data;
};

const uint32 ZipEntry::kFileHeaderSignature = 0x04034b50;
const uint32 ZipEntry::kDataDescriptorSignature = 0x08074b50;
const uint32 ZipEntry::kCentralDirSignature = 0x02014b50;
const uint32 ZipEntry::kEndOfCentralDirSignature = 0x06054b50;

bool UnzipEntry(const base::FilePath& unzip_dir,
                const std::string& bytes,
                std::string* error_msg) {
  ZipEntry entry;
  std::string zip_error_msg;
  if (!ZipEntry::FromBytes(bytes, &entry, &zip_error_msg)) {
    *error_msg = "Error while reading zip entry: " + zip_error_msg;
    return false;
  }
  std::string archive = entry.ToZip();
  return UnzipArchive(unzip_dir, archive, error_msg);
}

}  // namespace

bool UnzipSoleFile(const base::FilePath& unzip_dir,
                   const std::string& bytes,
                   base::FilePath* file,
                   std::string* error_msg) {
  std::string archive_error, entry_error;
  if (!UnzipArchive(unzip_dir, bytes, &archive_error) &&
      !UnzipEntry(unzip_dir, bytes, &entry_error)) {
    *error_msg = base::StringPrintf(
        "Failed to unzip file: Archive error: (%s) Entry error: (%s)",
        archive_error.c_str(), entry_error.c_str());
    return false;
  }

  file_util::FileEnumerator enumerator(unzip_dir, false /* recursive */,
      file_util::FileEnumerator::FILES |
      file_util::FileEnumerator::DIRECTORIES);
  base::FilePath first_file = enumerator.Next();
  if (first_file.empty()) {
    *error_msg = "Zip contained 0 files";
    return false;
  }
  base::FilePath second_file = enumerator.Next();
  if (!second_file.empty()) {
    *error_msg = "Zip contained multiple files";
    return false;
  }
  *file = first_file;
  return true;
}

std::string JsonStringify(const Value* value) {
  std::string json;
  base::JSONWriter::Write(value, &json);
  return json;
}

namespace {

// Truncates the given string to 100 characters, adding an ellipsis if
// truncation was necessary.
void TruncateString(std::string* data) {
  const size_t kMaxLength = 100;
  if (data->length() > kMaxLength) {
    data->resize(kMaxLength);
    data->replace(kMaxLength - 3, 3, "...");
  }
}

// Truncates all strings contained in the given value.
void TruncateContainedStrings(Value* value) {
  ListValue* list;
  if (value->IsType(Value::TYPE_DICTIONARY)) {
    DictionaryValue* dict = static_cast<DictionaryValue*>(value);
    DictionaryValue::key_iterator key = dict->begin_keys();
    for (; key != dict->end_keys(); ++key) {
      Value* child;
      if (!dict->GetWithoutPathExpansion(*key, &child))
        continue;
      std::string data;
      if (child->GetAsString(&data)) {
        TruncateString(&data);
        dict->SetWithoutPathExpansion(*key, new base::StringValue(data));
      } else {
        TruncateContainedStrings(child);
      }
    }
  } else if (value->GetAsList(&list)) {
    for (size_t i = 0; i < list->GetSize(); ++i) {
      Value* child;
      if (!list->Get(i, &child))
        continue;
      std::string data;
      if (child->GetAsString(&data)) {
        TruncateString(&data);
        list->Set(i, new base::StringValue(data));
      } else {
        TruncateContainedStrings(child);
      }
    }
  }
}

}  // namespace

std::string JsonStringifyForDisplay(const Value* value) {
  scoped_ptr<Value> copy;
  if (value->IsType(Value::TYPE_STRING)) {
    std::string data;
    value->GetAsString(&data);
    TruncateString(&data);
    copy.reset(new base::StringValue(data));
  } else {
    copy.reset(value->DeepCopy());
    TruncateContainedStrings(copy.get());
  }
  std::string json;
  base::JSONWriter::WriteWithOptions(copy.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json);
  return json;
}

const char* GetJsonTypeName(Value::Type type) {
  switch (type) {
    case Value::TYPE_NULL:
      return "null";
    case Value::TYPE_BOOLEAN:
      return "boolean";
    case Value::TYPE_INTEGER:
      return "integer";
    case Value::TYPE_DOUBLE:
      return "double";
    case Value::TYPE_STRING:
      return "string";
    case Value::TYPE_BINARY:
      return "binary";
    case Value::TYPE_DICTIONARY:
      return "dictionary";
    case Value::TYPE_LIST:
      return "list";
  }
  return "unknown";
}

std::string AutomationIdToString(const AutomationId& id) {
  return base::StringPrintf("%d-%s", id.type(), id.id().c_str());
}

bool StringToAutomationId(const std::string& string_id, AutomationId* id) {
  std::vector<std::string> split_id;
  base::SplitString(string_id, '-', &split_id);
  if (split_id.size() != 2)
    return false;
  int type;
  if (!base::StringToInt(split_id[0], &type))
    return false;
  *id = AutomationId(static_cast<AutomationId::Type>(type), split_id[1]);
  return true;
}

std::string WebViewIdToString(const WebViewId& view_id) {
  return base::StringPrintf(
      "%s%s",
      view_id.old_style() ? "t" : "f",
      AutomationIdToString(view_id.GetId()).c_str());
}

bool StringToWebViewId(const std::string& string_id, WebViewId* view_id) {
  if (string_id.empty() || (string_id[0] != 'f' && string_id[0] != 't'))
    return false;
  bool old_style = string_id[0] == 't';
  AutomationId id;
  if (!StringToAutomationId(string_id.substr(1), &id))
    return false;

  if (old_style) {
    int tab_id;
    if (!base::StringToInt(id.id(), &tab_id))
      return false;
    *view_id = WebViewId::ForOldStyleTab(tab_id);
  } else {
    *view_id = WebViewId::ForView(id);
  }
  return true;
}

Error* FlattenStringArray(const ListValue* src, string16* dest) {
  string16 keys;
  for (size_t i = 0; i < src->GetSize(); ++i) {
    string16 keys_list_part;
    src->GetString(i, &keys_list_part);
    for (size_t j = 0; j < keys_list_part.size(); ++j) {
      if (CBU16_IS_SURROGATE(keys_list_part[j])) {
        return new Error(kBadRequest,
                         "ChromeDriver only supports characters in the BMP");
      }
    }
    keys.append(keys_list_part);
  }
  *dest = keys;
  return NULL;
}

ValueParser::ValueParser() { }

ValueParser::~ValueParser() { }

}  // namespace webdriver

bool ValueConversionTraits<webdriver::SkipParsing>::SetFromValue(
    const Value* value, const webdriver::SkipParsing* t) {
  return true;
}

bool ValueConversionTraits<webdriver::SkipParsing>::CanConvert(
    const Value* value) {
  return true;
}
