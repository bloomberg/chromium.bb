// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_IBUS_IBUS_OBJECT_H_
#define CHROMEOS_DBUS_IBUS_IBUS_OBJECT_H_

#include <string>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chromeos/chromeos_export.h"

namespace dbus {
class MessageReader;
class MessageWriter;
}  // dbus

namespace chromeos {

// The data structure of IBusObject is represented as variant in "(sav...)"
// signatur. The IBusObject is constructed with two sections, header and
// contents. The header section is represent as "sav" which contains type name
// and attachement array. The contents section is corresponding to "..." in
// above signature, which can store arbitary type values including IBusObject.
//
// DATA STRUCTURE OVERVIEW:
//
// variant  struct {
//     // Header section
//     string typename  // The type name of object, like "IBusText"
//     array []  // attachement array.
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
// The IBusObjectReader class provides reading IBusObject from dbus message.
// This class checks the IBusObject header structure and type name before
// reading contents.
//
// EXAPMLE USAGE:
//   // Craetes reader for IBusText
//   IBusObjectReader object_reader("IBusText", &reader);
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
  // Returns true on success.
  bool Init();

  // Reads IBusOBject with |reader| and checks if the type name is valid.
  bool InitWithParentReader(dbus::MessageReader* reader);

  // Returns true if the IBusObject is valid.
  bool IsValid() const;

  // The following functions delegate dbus::MessageReader's functions.
  bool PopString(std::string* out);
  bool PopUint32(uint32* out);
  bool PopArray(dbus::MessageReader* reader);
  bool PopIBusObject(IBusObjectReader* reader);
  bool HasMoreData();

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

  DISALLOW_COPY_AND_ASSIGN(IBusObjectReader);
};

// IBusObjectWriter class provides writing IBusObject to dbus message. This
// class appends header section before appending contents values.
//
// EXAMPLE USAGE:
//   // Creates writer for IBusText
//   IBusObjectWriter object_writer("IBusText", "sv", &writer);
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
  // |writer| must be released by caller.
  IBusObjectWriter(const std::string& type_name,
                   const std::string& signature,
                   dbus::MessageWriter* writer);
  virtual ~IBusObjectWriter();

  // Appends IBusObject headers with |writer|, should be called once.
  void InitWithParentWriter(dbus::MessageWriter* writer);

  // The following functions delegate dbus::MessageReader's functions.
  void AppendString(const std::string& input);
  void AppendUint32(uint32 value);
  void OpenArray(const std::string& signature,
                 dbus::MessageWriter* writer);
  void CloseContainer(dbus::MessageWriter* writer);

  // Sets up |writer| for writing new IBusObject entry.
  void AppendIBusObject(IBusObjectWriter* writer);

  // Closes all opened containers.
  void CloseAll();

  // Returns true if writer is initialized.
  bool IsInitialized() const;

 private:
  friend class TestableIBusObjectWriter;
  // Appends IBusObject headers, should be called once.
  void Init();

  std::string type_name_;
  std::string signature_;
  dbus::MessageWriter* original_writer_;
  scoped_ptr<dbus::MessageWriter> top_variant_writer_;
  scoped_ptr<dbus::MessageWriter> contents_writer_;

  DISALLOW_COPY_AND_ASSIGN(IBusObjectWriter);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_IBUS_IBUS_OBJECT_H_
