// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MOCK_PRINTER_H_
#define CHROME_RENDERER_MOCK_PRINTER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "printing/image.h"
#include "ui/gfx/size.h"

struct ViewMsg_Print_Params;
struct ViewMsg_PrintPages_Params;
struct ViewHostMsg_DidPrintPage_Params;

// A class which represents an output page used in the MockPrinter class.
// The MockPrinter class stores output pages in a vector, so, this class
// inherits the base::RefCounted<> class so that the MockPrinter class can use
// a smart pointer of this object (i.e. scoped_refptr<>).
class MockPrinterPage : public base::RefCounted<MockPrinterPage> {
 public:
  MockPrinterPage(const void* source_data,
                  uint32 source_size,
                  const printing::Image& image);

  int width() const { return image_.size().width(); }
  int height() const { return image_.size().height(); }
  const uint8* source_data() const { return source_data_.get(); }
  uint32 source_size() const { return source_size_; }
  const printing::Image& image() const { return image_; }

 private:
  friend class base::RefCounted<MockPrinterPage>;
  virtual ~MockPrinterPage();

  uint32 source_size_;
  scoped_array<uint8> source_data_;
  printing::Image image_;

  DISALLOW_COPY_AND_ASSIGN(MockPrinterPage);
};

// A class which implements a pseudo-printer object used by the RenderViewTest
// class.
// This class consists of three parts:
// 1. An IPC-message hanlder sent from the RenderView class;
// 2. A renderer that creates a printing job into bitmaps, and;
// 3. A vector which saves the output pages of a printing job.
// A user who writes RenderViewTest cases only use the functions which
// retrieve output pages from this vector to verify them with expected results.
class MockPrinter {
 public:
  enum Status {
    PRINTER_READY,
    PRINTER_PRINTING,
    PRINTER_ERROR,
  };

  MockPrinter();
  ~MockPrinter();

  // Functions that changes settings of a pseudo printer.
  void ResetPrinter();
  void SetDefaultPrintSettings(const ViewMsg_Print_Params& params);

  // Functions that handles IPC events.
  void GetDefaultPrintSettings(ViewMsg_Print_Params* params);
  void ScriptedPrint(int cookie,
                     int expected_pages_count,
                     bool has_selection,
                     ViewMsg_PrintPages_Params* settings);
  void SetPrintedPagesCount(int cookie, int number_pages);
  void PrintPage(const ViewHostMsg_DidPrintPage_Params& params);

  // Functions that retrieve the output pages.
  Status GetPrinterStatus() const { return printer_status_; }
  int GetPrintedPages() const;

  // Get a pointer to the printed page, returns NULL if pageno has not been
  // printed.  The pointer is for read only view and should not be deleted.
  const MockPrinterPage* GetPrintedPage(unsigned int pageno) const;

  int GetWidth(unsigned int page) const;
  int GetHeight(unsigned int page) const;
  bool GetBitmapChecksum(unsigned int page, std::string* checksum) const;
  bool GetSource(unsigned int page, const void** data, uint32* size) const;
  bool GetBitmap(unsigned int page, const void** data, uint32* size) const;
  bool SaveSource(unsigned int page, const FilePath& filepath) const;
  bool SaveBitmap(unsigned int page, const FilePath& filepath) const;

 protected:
  int CreateDocumentCookie();
  bool GetChecksum(const void* data, uint32 size, std::string* checksum) const;

 private:
  // In pixels according to dpi_x and dpi_y.
  gfx::Size page_size_;
  gfx::Size printable_size_;
  int margin_left_;
  int margin_top_;

  // Specifies dots per inch.
  double dpi_;
  double max_shrink_;
  double min_shrink_;

  // Desired apparent dpi on paper.
  int desired_dpi_;

  // Print selection.
  bool selection_only_;

  // Cookie for the document to ensure correctness.
  int document_cookie_;
  int current_document_cookie_;

  // The current status of this printer.
  Status printer_status_;

  // The output of a printing job.
  int number_pages_;
  int page_number_;
  std::vector<scoped_refptr<MockPrinterPage> > pages_;

  DISALLOW_COPY_AND_ASSIGN(MockPrinter);
};

#endif  // CHROME_RENDERER_MOCK_PRINTER_H_
