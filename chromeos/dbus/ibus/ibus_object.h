// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_OBJECT_H_
#define CHROMEOS_DBUS_IBUS_IBUS_OBJECT_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"
#include "chromeos/dbus/ibus/ibus_property.h"
#include "chromeos/dbus/ibus/ibus_text.h"

namespace base {
class Value;
}  // namespace base

namespace dbus {
class MessageReader;
class MessageWriter;
}  // namespace dbus

namespace chromeos {

// The data structure of IBusObject is represented as variant in "(sav...)"
// signature. The IBusObject is constructed with two sections, header and
// contents. The header section is represent as "sav" which contains type name
// and attachment array. The contents section is corresponding to "..." in
// above signature, which can store arbitrary type values including IBusObject.
//
// DATA STRUCTURE OVERVIEW:
//
// variant  // Handle with top_variant_writer_/top_variant_reader_.
//   struct {  // Handle with contents_writer_/contents_reader_.
//     // Header section
//     string typename  // The type name of object, like "IBusText"
//     array [  // attachment array.
//       dict_entry (
//         string "mozc.candidates"  // The key in the dictionary entry.
//         variant ...  // The value in the dictionary entry.
//       )
//       ...
//     ]
//
//     // Contents section
//     ...  // The contents area.
//   }
//
// EXAMPLE: IBusText
//
// variant  struct {
//      string "IBusText"  // Header of IBusText
//      array[]
//      string "A"  // The 1st value of IBusText
//      variant  struct {  // THe 2nd value of IBusText
//          string "IBusAttrList"  // Header of IBusAttrList
//          array[]
//          array[  // The 1st value of IBusAttrList
//            variant  struct{
//                string "IBusAttribute"  // Header of IBusAttribute
//                array[]
//                uint32 1  // The 1st value of IBusAttribute
//                uint32 1  // The 2nd value of IBusAttribute
//                uint32 0  // The 3rd value of IBusAttribute
//                uint32 1  // The 4th value of IBusAttribute
//              }
//          ]
//        }
//   }
//
// The IBusObjectReader class provides reading IBusObject including attachment
// field from dbus message. This class checks the IBusObject header structure
// and type name before reading contents.
//
// EXAPMLE USAGE:
//   // Creates reader for IBusText
//   IBusObjectReader object_reader("IBusText", &reader);
//
//   // Initialize for reading attachment field.
//   object_reader.Init();
//
//   // Get attachment field.
//   base::Value* value = object_reader.GetAttachment("annotation");
//
//   std::string text;
//   reader.PopString(&text);  // Reading 1st value as string.
//
//   // We can also read nested IBusObject.
//   IBusObjectReader nested_object_reader("IBusAttrList", NULL);
//   reader.PopIBusObject(&nested_object_reader);
class CHROMEOS_EXPORT IBusObjectReader {
 public:
  // |reader| must be released by caller.
  IBusObjectReader(const std::string& type_name,
                   dbus::MessageReader* reader);
  virtual ~IBusObjectReader();

  // Reads IBusObject headers and checks if the type name is valid.
  // Returns true on success. Uses InitWitAttachmentReader instead if you want
  // to read attachment field.
  bool Init();

  // Reads IBusOBject with |reader| and checks if the type name is valid.
  bool InitWithParentReader(dbus::MessageReader* reader);

  // Returns true if the IBusObject is valid.
  bool IsValid() const;

  // The following functions delegate dbus::MessageReader's functions.
  bool PopString(std::string* out);
  bool PopUint32(uint32* out);
  bool PopArray(dbus::MessageReader* reader);
  bool PopBool(bool* out);
  bool PopInt32(int32* out);
  bool HasMoreData();

  // Sets up |reader| for reading an IBusObject entry.
  bool PopIBusObject(IBusObjectReader* reader);

  // Pops a IBusText.
  // Returns true on success.
  bool PopIBusText(IBusText* text);

  // Pops a IBusText and store it's text field into |text|. Use PopIBusText
  // instead in the case of using any attribute entries in IBusText.
  // Return true on success.
  bool PopStringFromIBusText(std::string* text);

  // Pops a IBusProperty.
  bool PopIBusProperty(IBusProperty* property);

  // Pops a IBusPropertyList.
  bool PopIBusPropertyList(IBusPropertyList* property_list);

  // Gets attachment entry corresponding to |key|. Do not free returned value.
  // Returns NULL if there is no entry.
  const base::Value* GetAttachment(const std::string& key);

 private:
  enum CheckResult {
    IBUS_OBJECT_VALID,  // Already checked and valid type.
    IBUS_OBJECT_INVALID,  // Already checked but invalid type.
    IBUS_OBJECT_NOT_CHECKED,  // Not checked yet.
  };

  std::string type_name_;
  dbus::MessageReader* original_reader_;
  scoped_ptr<dbus::MessageReader> top_variant_reader_;
  scoped_ptr<dbus::MessageReader> contents_reader_;
  CheckResult check_result_;
  std::map<std::string, base::Value*> attachments_;

  DISALLOW_COPY_AND_ASSIGN(IBusObjectReader);
};

// IBusObjectWriter class provides writing IBusObject to dbus message. This
// class appends header section before appending contents values.
// IBusObjectWriter does not support writing attachment field because writing
// attachment field is not used in Chrome.
//
// EXAMPLE USAGE:
//   // Creates writer for IBusText
//   IBusObjectWriter object_writer("IBusText", "sv", &writer);
//
//   // Add some attachments.
//   base::Value* value = base::Value::CreateStringValue("Noun");
//   object_writer.AddAttachment("annotation", *value);
//
//   // Close header section.
//   object_writer.CloseHeader();
//
//   const std::string text = "Sample Text";
//   writer.AppendString(text);
//
//   // We can also write nested IBusObject.
//   IBusObjectWriter nested_object_writer("IBusAttrList", "av");
//   object_writer.AppendIBusObject(&nested_object_writer);
//   ... appends values
//
//   nested_object_writer.CloseAll();  // To finish up, should call CloseAll.
//   object_writer.CloseAll();
class CHROMEOS_EXPORT IBusObjectWriter {
 public:
  enum WriterState {
    NOT_INITIALZED,  // Created but not initialized.
    HEADER_OPEN,  // Ready for writing attachment field.
    INITIALIZED  // Ready for writing content values.
  };

  // |writer| must be released by caller.
  IBusObjectWriter(const std::string& type_name,
                   const std::string& signature,
                   dbus::MessageWriter* writer);
  virtual ~IBusObjectWriter();

  // Closes header to write content values.
  void CloseHeader();

  // Appends IBusObject headers with |writer|, should be called once.
  void InitWithParentWriter(dbus::MessageWriter* writer);

  // Adds an attachment, this function can be called only before CloseHeader
  // function call.
  bool AddAttachment(const std::string& key, const base::Value& value);

  // The following functions delegate dbus::MessageReader's functions.
  void AppendString(const std::string& input);
  void AppendUint32(uint32 value);
  void AppendInt32(int32 value);
  void AppendBool(bool value);
  void OpenArray(const std::string& signature,
                 dbus::MessageWriter* writer);
  void CloseContainer(dbus::MessageWriter* writer);

  // Sets up |writer| for writing new IBusObject entry.
  void AppendIBusObject(IBusObjectWriter* writer);

  // Closes all opened containers.
  void CloseAll();

  // Appends a IBusText.
  void AppendIBusText(const IBusText& text);

  // Appends a string as IBusText without any attributes. Use AppendIBusText
  // instead in the case of using any attribute entries.
  void AppendStringAsIBusText(const std::string& text);

  // Appends a IBusProperty.
  void AppendIBusProperty(const IBusProperty& property);

  // Appends a IBusPropertyList.
  void AppendIBusPropertyList(const IBusPropertyList& property_list);

 private:
  // Appends IBusObject headers, should be called once.
  void Init();

  std::string type_name_;
  std::string signature_;
  dbus::MessageWriter* original_writer_;
  WriterState state_;
  scoped_ptr<dbus::MessageWriter> top_variant_writer_;
  scoped_ptr<dbus::MessageWriter> contents_writer_;
  scoped_ptr<dbus::MessageWriter> attachment_writer_;

  DISALLOW_COPY_AND_ASSIGN(IBusObjectWriter);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_OBJECT_H_
