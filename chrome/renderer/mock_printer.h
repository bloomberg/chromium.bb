// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MOCK_PRINTER_H_
#define CHROME_RENDERER_MOCK_PRINTER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "printing/image.h"
#include "ui/gfx/size.h"

struct PrintMsg_Print_Params;
struct PrintMsg_PrintPages_Params;
struct PrintHostMsg_DidPrintPage_Params;

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
  void SetDefaultPrintSettings(const PrintMsg_Print_Params& params);
  void UseInvalidSettings();

  // Functions that handles IPC events.
  void GetDefaultPrintSettings(PrintMsg_Print_Params* params);
  void ScriptedPrint(int cookie,
                     int expected_pages_count,
                     bool has_selection,
                     PrintMsg_PrintPages_Params* settings);
  void UpdateSettings(int cookie, PrintMsg_PrintPages_Params* params,
                      const std::vector<int>& page_range_array);
  void SetPrintedPagesCount(int cookie, int number_pages);
  void PrintPage(const PrintHostMsg_DidPrintPage_Params& params);

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

 private:
  // Helper function to fill the fields in |params|.
  void SetPrintParams(PrintMsg_Print_Params* params);

  // In pixels according to dpi_x and dpi_y.
  gfx::Size page_size_;
  gfx::Size content_size_;
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

  // Used only in the preview sequence.
  bool is_first_request_;
  int preview_request_id_;

  // Used for displaying headers and footers.
  bool display_header_footer_;
  string16 date_;
  string16 title_;
  string16 url_;

  // Used for generating invalid settings.
  bool use_invalid_settings_;

  std::vector<scoped_refptr<MockPrinterPage> > pages_;

  DISALLOW_COPY_AND_ASSIGN(MockPrinter);
};

#endif  // CHROME_RENDERER_MOCK_PRINTER_H_
