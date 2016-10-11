/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */
#include <wx/wx.h>
#include <wx/aboutdlg.h>
#include <wx/cmdline.h>
#include <wx/dcbuffer.h>
#include "./tools_common.h"
#include "./video_reader.h"
#include "aom/aom_decoder.h"
#include "aom/aomdx.h"
#include "av1/common/accounting.h"
#include "av1/common/onyxc_int.h"

#define OD_SIGNMASK(a) (-((a) < 0))
#define OD_FLIPSIGNI(a, b) (((a) + OD_SIGNMASK(b)) ^ OD_SIGNMASK(b))
#define OD_DIV_ROUND(x, y) (((x) + OD_FLIPSIGNI((y) >> 1, x)) / (y))

class AV1Decoder {
 private:
  FILE *input;
  wxString path;

  AvxVideoReader *reader;
  const AvxVideoInfo *info;
  const AvxInterface *decoder;

  aom_codec_ctx_t codec;

 public:
  aom_image_t *image;
  int frame;

  AV1Decoder();
  ~AV1Decoder();

  bool open(const wxString &path);
  void close();
  bool step();

  int getWidth() const;
  int getHeight() const;

  bool setAccountingEnabled(bool enable);
  bool getAccountingStruct(Accounting **acct);
};

AV1Decoder::AV1Decoder()
    : reader(NULL), info(NULL), decoder(NULL), image(NULL), frame(0) {}

AV1Decoder::~AV1Decoder() {}

bool AV1Decoder::open(const wxString &path) {
  reader = aom_video_reader_open(path.mb_str());
  if (!reader) {
    fprintf(stderr, "Failed to open %s for reading.", path.mb_str().data());
    return false;
  }
  this->path = path;
  info = aom_video_reader_get_info(reader);
  decoder = get_aom_decoder_by_fourcc(info->codec_fourcc);
  if (!decoder) {
    fprintf(stderr, "Unknown input codec.");
    return false;
  }
  printf("Using %s\n", aom_codec_iface_name(decoder->codec_interface()));
  if (aom_codec_dec_init(&codec, decoder->codec_interface(), NULL, 0)) {
    fprintf(stderr, "Failed to initialize decoder.");
    return false;
  }
  return true;
}

void AV1Decoder::close() {}

bool AV1Decoder::step() {
  if (aom_video_reader_read_frame(reader)) {
    size_t frame_size;
    const unsigned char *frame_data;
    frame_data = aom_video_reader_get_frame(reader, &frame_size);
    if (aom_codec_decode(&codec, frame_data, frame_size, NULL, 0)) {
      fprintf(stderr, "Failed to decode frame.");
      return false;
    } else {
      aom_codec_iter_t iter = NULL;
      image = aom_codec_get_frame(&codec, &iter);
      if (image != NULL) {
        frame++;
        return true;
      }
      return false;
    }
  }
  return false;
}

int AV1Decoder::getWidth() const { return info->frame_width; }

int AV1Decoder::getHeight() const { return info->frame_height; }

bool AV1Decoder::getAccountingStruct(Accounting **accounting) {
  return aom_codec_control(&codec, AV1_GET_ACCOUNTING, accounting) ==
         AOM_CODEC_OK;
}

#define MIN_ZOOM (1)
#define MAX_ZOOM (4)

class AnalyzerPanel : public wxPanel {
  DECLARE_EVENT_TABLE()

 private:
  AV1Decoder decoder;
  const wxString path;

  int zoom;
  unsigned char *pixels;

  const bool bit_accounting;
  double *bpp_q3;

  // The display size is the decode size, scaled by the zoom.
  int getDisplayWidth() const;
  int getDisplayHeight() const;

  bool updateDisplaySize();

  void computeBitsPerPixel();

 public:
  AnalyzerPanel(wxWindow *parent, const wxString &path,
                const bool bit_accounting);
  ~AnalyzerPanel();

  bool open(const wxString &path);
  void close();
  void render();
  bool nextFrame();
  void refresh();

  int getZoom() const;
  bool setZoom(int zoom);

  void onPaint(wxPaintEvent &event);  // NOLINT
};

BEGIN_EVENT_TABLE(AnalyzerPanel, wxPanel)
EVT_PAINT(AnalyzerPanel::onPaint)
END_EVENT_TABLE()

AnalyzerPanel::AnalyzerPanel(wxWindow *parent, const wxString &path,
                             const bool bit_accounting)
    : wxPanel(parent), path(path), zoom(0), pixels(NULL),
      bit_accounting(bit_accounting), bpp_q3(NULL) {}

AnalyzerPanel::~AnalyzerPanel() { close(); }

void AnalyzerPanel::render() {
  aom_image_t *img = decoder.image;
  int y_stride = img->stride[0];
  int cb_stride = img->stride[1];
  int cr_stride = img->stride[2];
  int p_stride = 3 * getDisplayWidth();
  unsigned char *y_row = img->planes[0];
  unsigned char *cb_row = img->planes[1];
  unsigned char *cr_row = img->planes[2];
  unsigned char *p_row = pixels;
  for (int j = 0; j < decoder.getHeight(); j++) {
    unsigned char *y = y_row;
    unsigned char *cb = cb_row;
    unsigned char *cr = cr_row;
    unsigned char *p = p_row;
    for (int i = 0; i < decoder.getWidth(); i++) {
      int64_t yval;
      int64_t cbval;
      int64_t crval;
      unsigned rval;
      unsigned gval;
      unsigned bval;
      yval = *y;
      cbval = *cb;
      crval = *cr;
      yval -= 16;
      cbval -= 128;
      crval -= 128;
      /*This is intentionally slow and very accurate.*/
      rval = OD_CLAMPI(0, (int32_t)OD_DIV_ROUND(
                              2916394880000LL * yval + 4490222169144LL * crval,
                              9745792000LL),
                       65535);
      gval = OD_CLAMPI(0, (int32_t)OD_DIV_ROUND(2916394880000LL * yval -
                                                    534117096223LL * cbval -
                                                    1334761232047LL * crval,
                                                9745792000LL),
                       65535);
      bval = OD_CLAMPI(0, (int32_t)OD_DIV_ROUND(
                              2916394880000LL * yval + 5290866304968LL * cbval,
                              9745792000LL),
                       65535);
      unsigned char *px_row = p;
      for (int v = 0; v < zoom; v++) {
        unsigned char *px = px_row;
        for (int u = 0; u < zoom; u++) {
          *(px + 0) = (unsigned char)(rval >> 8);
          *(px + 1) = (unsigned char)(gval >> 8);
          *(px + 2) = (unsigned char)(bval >> 8);
          px += 3;
        }
        px_row += p_stride;
      }
      int dc = ((y - y_row) & 1) | (1 - img->x_chroma_shift);
      y++;
      cb += dc;
      cr += dc;
      p += zoom * 3;
    }
    int dc = -((j & 1) | (1 - img->y_chroma_shift));
    y_row += y_stride;
    cb_row += dc & cb_stride;
    cr_row += dc & cr_stride;
    p_row += zoom * p_stride;
  }
}

void AnalyzerPanel::computeBitsPerPixel() {
  Accounting *acct;
  double bpp_total;
  int totals_q3[MAX_SYMBOL_TYPES] = { 0 };
  int sym_count[MAX_SYMBOL_TYPES] = { 0 };
  decoder.getAccountingStruct(&acct);
  for (int j = 0; j < decoder.getHeight(); j++) {
    for (int i = 0; i < decoder.getWidth(); i++) {
      bpp_q3[j * decoder.getWidth() + i] = 0.0;
    }
  }
  bpp_total = 0;
  for (int i = 0; i < acct->syms.num_syms; i++) {
    AccountingSymbol *s;
    s = &acct->syms.syms[i];
    totals_q3[s->id] += s->bits;
    sym_count[s->id] += s->samples;
  }
  printf("=== Frame: %-3i ===\n", decoder.frame - 1);
  for (int i = 0; i < acct->syms.dictionary.num_strs; i++) {
    if (totals_q3[i]) {
      printf("%30s = %10.3f (%f bit/symbol)\n", acct->syms.dictionary.strs[i],
             (float)totals_q3[i] / 8, (float)totals_q3[i] / 8 / sym_count[i]);
    }
  }
  printf("\n");
}

bool AnalyzerPanel::nextFrame() {
  if (decoder.step()) {
    refresh();
    return true;
  }
  return false;
}

void AnalyzerPanel::refresh() {
  if (bit_accounting) {
    computeBitsPerPixel();
  }
  render();
}

int AnalyzerPanel::getDisplayWidth() const { return zoom * decoder.getWidth(); }

int AnalyzerPanel::getDisplayHeight() const {
  return zoom * decoder.getHeight();
}

bool AnalyzerPanel::updateDisplaySize() {
  unsigned char *p = (unsigned char *)malloc(
      sizeof(*p) * 3 * getDisplayWidth() * getDisplayHeight());
  if (p == NULL) {
    return false;
  }
  free(pixels);
  pixels = p;
  SetSize(getDisplayWidth(), getDisplayHeight());
  return true;
}

bool AnalyzerPanel::open(const wxString &path) {
  if (!decoder.open(path)) {
    return false;
  }
  if (!setZoom(MIN_ZOOM)) {
    return false;
  }
  if (bit_accounting) {
    bpp_q3 = (double *)malloc(sizeof(*bpp_q3) * decoder.getWidth() *
                              decoder.getHeight());
    if (bpp_q3 == NULL) {
      fprintf(stderr, "Could not allocate memory for bit accounting\n");
      close();
      return false;
    }
  }
  if (!nextFrame()) {
    close();
    return false;
  }
  SetFocus();
  return true;
}

void AnalyzerPanel::close() {
  decoder.close();
  free(pixels);
  pixels = NULL;
  free(bpp_q3);
  bpp_q3 = NULL;
}

int AnalyzerPanel::getZoom() const { return zoom; }

bool AnalyzerPanel::setZoom(int z) {
  if (z <= MAX_ZOOM && z >= MIN_ZOOM && zoom != z) {
    int old_zoom = zoom;
    zoom = z;
    if (!updateDisplaySize()) {
      zoom = old_zoom;
      return false;
    }
    return true;
  }
  return false;
}

void AnalyzerPanel::onPaint(wxPaintEvent &) {
  wxBitmap bmp(wxImage(getDisplayWidth(), getDisplayHeight(), pixels, true));
  wxBufferedPaintDC dc(this, bmp);
}

class AnalyzerFrame : public wxFrame {
  DECLARE_EVENT_TABLE()

 private:
  AnalyzerPanel *panel;
  const bool bit_accounting;

  wxMenu *fileMenu;
  wxMenu *viewMenu;
  wxMenu *playbackMenu;

 public:
  AnalyzerFrame(const bool bit_accounting);  // NOLINT

  void onOpen(wxCommandEvent &event);   // NOLINT
  void onClose(wxCommandEvent &event);  // NOLINT
  void onQuit(wxCommandEvent &event);   // NOLINT

  void onZoomIn(wxCommandEvent &event);      // NOLINT
  void onZoomOut(wxCommandEvent &event);     // NOLINT
  void onActualSize(wxCommandEvent &event);  // NOLINT

  void onNextFrame(wxCommandEvent &event);  // NOLINT
  void onGotoFrame(wxCommandEvent &event);  // NOLINT
  void onRestart(wxCommandEvent &event);    // NOLINT

  void onAbout(wxCommandEvent &event);  // NOLINT

  bool open(const wxString &path);
  bool setZoom(int zoom);
};

enum {
  wxID_NEXT_FRAME = 6000,
  wxID_GOTO_FRAME,
  wxID_RESTART,
  wxID_ACTUAL_SIZE
};

BEGIN_EVENT_TABLE(AnalyzerFrame, wxFrame)
EVT_MENU(wxID_OPEN, AnalyzerFrame::onOpen)
EVT_MENU(wxID_CLOSE, AnalyzerFrame::onClose)
EVT_MENU(wxID_EXIT, AnalyzerFrame::onQuit)
EVT_MENU(wxID_ZOOM_IN, AnalyzerFrame::onZoomIn)
EVT_MENU(wxID_ZOOM_OUT, AnalyzerFrame::onZoomOut)
EVT_MENU(wxID_ACTUAL_SIZE, AnalyzerFrame::onActualSize)
EVT_MENU(wxID_NEXT_FRAME, AnalyzerFrame::onNextFrame)
EVT_MENU(wxID_GOTO_FRAME, AnalyzerFrame::onGotoFrame)
EVT_MENU(wxID_RESTART, AnalyzerFrame::onRestart)
EVT_MENU(wxID_ABOUT, AnalyzerFrame::onAbout)
END_EVENT_TABLE()

AnalyzerFrame::AnalyzerFrame(const bool bit_accounting)
    : wxFrame(NULL, wxID_ANY, _("AV1 Stream Analyzer"), wxDefaultPosition,
              wxDefaultSize, wxDEFAULT_FRAME_STYLE),
      panel(NULL), bit_accounting(bit_accounting) {
  wxMenuBar *mb = new wxMenuBar();

  fileMenu = new wxMenu();
  fileMenu->Append(wxID_OPEN, _("&Open...\tCtrl-O"), _("Open daala file"));
  fileMenu->Append(wxID_CLOSE, _("&Close\tCtrl-W"), _("Close daala file"));
  fileMenu->Enable(wxID_CLOSE, false);
  fileMenu->Append(wxID_EXIT, _("E&xit\tCtrl-Q"), _("Quit this program"));
  mb->Append(fileMenu, _("&File"));

  wxAcceleratorEntry entries[2];
  entries[0].Set(wxACCEL_CTRL, (int)'=', wxID_ZOOM_IN);
  entries[1].Set(wxACCEL_CTRL | wxACCEL_SHIFT, (int)'-', wxID_ZOOM_OUT);
  wxAcceleratorTable accel(2, entries);
  this->SetAcceleratorTable(accel);

  viewMenu = new wxMenu();
  viewMenu->Append(wxID_ZOOM_IN, _("Zoom-In\tCtrl-+"), _("Double image size"));
  viewMenu->Append(wxID_ZOOM_OUT, _("Zoom-Out\tCtrl--"), _("Half image size"));
  viewMenu->Append(wxID_ACTUAL_SIZE, _("Actual size\tCtrl-0"),
                   _("Actual size of the frame"));
  mb->Append(viewMenu, _("&View"));

  playbackMenu = new wxMenu();
  playbackMenu->Append(wxID_NEXT_FRAME, _("Next frame\tCtrl-."),
                       _("Go to next frame"));
  /*playbackMenu->Append(wxID_RESTART, _("&Restart\tCtrl-R"),
                       _("Set video to frame 0"));
  playbackMenu->Append(wxID_GOTO_FRAME, _("Jump to Frame\tCtrl-J"),
                       _("Go to frame number"));*/
  mb->Append(playbackMenu, _("&Playback"));

  wxMenu *helpMenu = new wxMenu();
  helpMenu->Append(wxID_ABOUT, _("&About...\tF1"), _("Show about dialog"));
  mb->Append(helpMenu, _("&Help"));

  SetMenuBar(mb);

  CreateStatusBar(1);
}

void AnalyzerFrame::onOpen(wxCommandEvent &WXUNUSED(event)) {
  wxFileDialog openFileDialog(this, _("Open file"), wxEmptyString,
                              wxEmptyString, _("AV1 files (*.ivf)|*.ivf"),
                              wxFD_OPEN | wxFD_FILE_MUST_EXIST);
  if (openFileDialog.ShowModal() != wxID_CANCEL) {
    open(openFileDialog.GetPath());
  }
}

void AnalyzerFrame::onClose(wxCommandEvent &WXUNUSED(event)) {}

void AnalyzerFrame::onQuit(wxCommandEvent &WXUNUSED(event)) { Close(true); }

void AnalyzerFrame::onZoomIn(wxCommandEvent &WXUNUSED(event)) {
  setZoom(panel->getZoom() + 1);
}

void AnalyzerFrame::onZoomOut(wxCommandEvent &WXUNUSED(event)) {
  setZoom(panel->getZoom() - 1);
}

void AnalyzerFrame::onActualSize(wxCommandEvent &WXUNUSED(event)) {
  setZoom(MIN_ZOOM);
}

void AnalyzerFrame::onNextFrame(wxCommandEvent &WXUNUSED(event)) {
  panel->nextFrame();
  panel->Refresh(false);
}

void AnalyzerFrame::onGotoFrame(wxCommandEvent &WXUNUSED(event)) {}

void AnalyzerFrame::onRestart(wxCommandEvent &WXUNUSED(event)) {}

void AnalyzerFrame::onAbout(wxCommandEvent &WXUNUSED(event)) {
  wxAboutDialogInfo info;
  info.SetName(_("AV1 Bitstream Analyzer"));
  info.SetVersion(_("0.1-beta"));
  info.SetDescription(
      _("This program implements a bitstream analyzer for AV1"));
  info.SetCopyright(
      wxT("(C) 2017 Alliance for Open Media <negge@mozilla.com>"));
  wxAboutBox(info);
}

bool AnalyzerFrame::open(const wxString &path) {
  panel = new AnalyzerPanel(this, path, bit_accounting);
  if (panel->open(path)) {
    SetClientSize(panel->GetSize());
    return true;
  } else {
    delete panel;
    return false;
  }
}

bool AnalyzerFrame::setZoom(int zoom) {
  if (panel->setZoom(zoom)) {
    GetMenuBar()->Enable(wxID_ACTUAL_SIZE, zoom != MIN_ZOOM);
    GetMenuBar()->Enable(wxID_ZOOM_IN, zoom != MAX_ZOOM);
    GetMenuBar()->Enable(wxID_ZOOM_OUT, zoom != MIN_ZOOM);
    SetClientSize(panel->GetSize());
    panel->render();
    panel->Refresh();
    return true;
  }
  return false;
}

class Analyzer : public wxApp {
 private:
  AnalyzerFrame *frame;

 public:
  void OnInitCmdLine(wxCmdLineParser &parser);    // NOLINT
  bool OnCmdLineParsed(wxCmdLineParser &parser);  // NOLINT
};

static const wxCmdLineEntryDesc CMD_LINE_DESC[] = {
  { wxCMD_LINE_SWITCH, _("h"), _("help"), _("Display this help and exit."),
    wxCMD_LINE_VAL_NONE, wxCMD_LINE_OPTION_HELP },
  { wxCMD_LINE_SWITCH, _("a"), _("bit-accounting"), _("Enable bit accounting"),
    wxCMD_LINE_VAL_NONE, wxCMD_LINE_PARAM_OPTIONAL },
  { wxCMD_LINE_PARAM, NULL, NULL, _("input.ivf"), wxCMD_LINE_VAL_STRING,
    wxCMD_LINE_PARAM_OPTIONAL },
  { wxCMD_LINE_NONE }
};

void Analyzer::OnInitCmdLine(wxCmdLineParser &parser) {  // NOLINT
  parser.SetDesc(CMD_LINE_DESC);
  parser.SetSwitchChars(_("-"));
}

bool Analyzer::OnCmdLineParsed(wxCmdLineParser &parser) {  // NOLINT
  bool bit_accounting = parser.Found(_("a"));
  if (bit_accounting && !CONFIG_ACCOUNTING) {
    fprintf(stderr,
            "Bit accounting support not found.  "
            "Recompile with:\n./configure --enable-accounting\n");
    return false;
  }
  frame = new AnalyzerFrame(parser.Found(_("a")));
  frame->Show();
  if (parser.GetParamCount() > 0) {
    return frame->open(parser.GetParam(0));
  }
  return true;
}

void usage_exit(void) {
  fprintf(stderr, "uhh\n");
  exit(EXIT_FAILURE);
}

IMPLEMENT_APP(Analyzer)
