// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GFX_FONT_H_
#define APP_GFX_FONT_H_

#include "build/build_config.h"

#include <string>

#if defined(OS_LINUX)
#include <list>
#endif

#if defined(OS_WIN)
typedef struct HFONT__* HFONT;
#endif

#if defined(OS_WIN)
typedef struct HFONT__* NativeFont;
#elif defined(OS_MACOSX)
#ifdef __OBJC__
@class NSFont;
#else
class NSFont;
#endif
typedef NSFont* NativeFont;
#elif defined(OS_LINUX)
typedef struct _PangoFontDescription PangoFontDescription;
typedef PangoFontDescription* NativeFont;
#else  // null port.
#error No known OS defined
#endif

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "base/scoped_ptr.h"

namespace gfx {

// Font provides a wrapper around an underlying font. Copy and assignment
// operators are explicitly allowed, and cheap.
class Font {
 public:
  // The following constants indicate the font style.
  enum {
    NORMAL = 0,
    BOLD = 1,
    ITALIC = 2,
    UNDERLINED = 4,
  };

  // Creates a Font given font name (e.g. arial), font size (e.g. 12).
  // Skia actually expects a family name and not a font name.
  static Font CreateFont(const std::wstring& font_name, int font_size);

  ~Font() { }

  // Returns a new Font derived from the existing font.
  // size_deta is the size to add to the current font. For example, a value
  // of 5 results in a font 5 units bigger than this font.
  Font DeriveFont(int size_delta) const {
    return DeriveFont(size_delta, style());
  }

  // Returns a new Font derived from the existing font.
  // size_delta is the size to add to the current font. See the single
  // argument version of this method for an example.
  // The style parameter specifies the new style for the font, and is a
  // bitmask of the values: BOLD, ITALIC and UNDERLINED.
  Font DeriveFont(int size_delta, int style) const;

  // Returns the number of vertical pixels needed to display characters from
  // the specified font.
  int height() const;

  // Returns the baseline, or ascent, of the font.
  int baseline() const;

  // Returns the average character width for the font.
  int ave_char_width() const;

  // Returns the number of horizontal pixels needed to display the specified
  // string.
  int GetStringWidth(const std::wstring& text) const;

  // Returns the expected number of horizontal pixels needed to display
  // the specified length of characters.
  // Call GetStringWidth() to retrieve the actual number.
  int GetExpectedTextWidth(int length) const;

  // Returns the style of the font.
  int style() const;

  // Font Name.
  // It is actually a font family name, because Skia expects a family name
  // and not a font name.
  std::wstring FontName();

  // Font Size.
  int FontSize();

  // Returns a handle to the native font.
  // NOTE: on linux this returns the PangoFontDescription* being held by this
  // object. You should not modify or free it. If you need to use it, make a
  // copy of it by way of pango_font_description_copy(nativeFont()).
  NativeFont nativeFont() const;

  // Creates a font with the default name and style.
  Font();

#if defined(OS_WIN)
  // Creates a Font from the specified HFONT. The supplied HFONT is effectively
  // copied.
  static Font CreateFont(HFONT hfont);

  // Returns the handle to the underlying HFONT. This is used by gfx::Canvas to
  // draw text.
  HFONT hfont() const { return font_ref_->hfont(); }

  // Dialog units to pixels conversion.
  // See http://support.microsoft.com/kb/145994 for details.
  int horizontal_dlus_to_pixels(int dlus) {
    return dlus * font_ref_->dlu_base_x() / 4;
  }
  int vertical_dlus_to_pixels(int dlus) {
    return dlus * font_ref_->height() / 8;
  }
#elif defined(OS_LINUX)
  static Font CreateFont(PangoFontDescription* desc);
#endif

 private:

#if defined(OS_WIN)
  // Chrome text drawing bottoms out in the Windows GDI functions that take an
  // HFONT (an opaque handle into Windows). To avoid lots of GDI object
  // allocation and destruction, Font indirectly refers to the HFONT by way of
  // an HFontRef. That is, every Font has an HFontRef, which has an HFONT.
  //
  // HFontRef is reference counted. Upon deletion, it deletes the HFONT.
  // By making HFontRef maintain the reference to the HFONT, multiple
  // HFontRefs can share the same HFONT, and Font can provide value semantics.
  class HFontRef : public base::RefCounted<HFontRef> {
   public:
    // This constructor takes control of the HFONT, and will delete it when
    // the HFontRef is deleted.
    HFontRef(HFONT hfont,
             int height,
             int baseline,
             int ave_char_width,
             int style,
             int dlu_base_x);
    ~HFontRef();

    // Accessors
    HFONT hfont() const { return hfont_; }
    int height() const { return height_; }
    int baseline() const { return baseline_; }
    int ave_char_width() const { return ave_char_width_; }
    int style() const { return style_; }
    int dlu_base_x() const { return dlu_base_x_; }

   private:
    const HFONT hfont_;
    const int height_;
    const int baseline_;
    const int ave_char_width_;
    const int style_;
    // Constants used in converting dialog units to pixels.
    const int dlu_base_x_;

    DISALLOW_COPY_AND_ASSIGN(HFontRef);
  };

  // Returns the base font ref. This should ONLY be invoked on the
  // UI thread.
  static HFontRef* GetBaseFontRef();

  // Creates and returns a new HFONTRef from the specified HFONT.
  static HFontRef* CreateHFontRef(HFONT font);

  explicit Font(HFontRef* font_ref) : font_ref_(font_ref) { }

  // Reference to the base font all fonts are derived from.
  static HFontRef* base_font_ref_;

  // Indirect reference to the HFontRef, which references the underlying HFONT.
  scoped_refptr<HFontRef> font_ref_;
#elif defined(OS_LINUX)
  // Used internally on Linux to cache information about the font. Each
  // Font maintains a reference to a PangoFontRef. Coping a Font ups the
  // refcount of the corresponding PangoFontRef.
  //
  // As obtaining metrics (height, ascent, average char width) is expensive,
  // the metrics are only obtained as needed. Additionally PangoFontRef
  // maintains a static cache of PangoFontRefs. When a PangoFontRef is needed
  // the cache is checked first.
  class PangoFontRef : public base::RefCounted<PangoFontRef> {
   public:
    ~PangoFontRef();

    // Creates or returns a cached PangoFontRef of the given family, size and
    // style.
    static PangoFontRef* Create(PangoFontDescription* pfd,
                                const std::wstring& family,
                                int size,
                                int style);

    PangoFontDescription* pfd() const { return pfd_; }
    const std::wstring& family() const { return family_; }
    int size() const { return size_; }
    int style() const { return style_; }
    int GetHeight() const;
    int GetAscent() const;
    int GetAveCharWidth() const;

   private:
    typedef std::list<PangoFontRef*> Cache;

    PangoFontRef(PangoFontDescription* pfd,
                 const std::wstring& family,
                 int size,
                 int style);

    void CalculateMetricsIfNecessary() const;

    // Returns the cache.
    static Cache* GetCache();

    // Adds |ref| to the cache, removing an existing PangoFontRef if there are
    // too many.
    static void AddToCache(PangoFontRef* ref);

    PangoFontDescription* pfd_;
    const std::wstring family_;
    const int size_;
    const int style_;

    // Metrics related members. As these are expensive to calculate they are
    // calculated the first time requested.
    mutable int height_;
    mutable int ascent_;
    mutable int ave_char_width_;

    // Have the metrics related members been determined yet (height_, ascent_
    // ave_char_width_)?
    mutable bool calculated_metrics_;

    DISALLOW_COPY_AND_ASSIGN(PangoFontRef);
  };

  explicit Font(PangoFontDescription* pfd);

  // The default font, used for the default constructor.
  static Font* default_font_;

  scoped_refptr<PangoFontRef> font_ref_;
#elif defined(OS_MACOSX)
  explicit Font(const std::wstring& font_name, int font_size, int style);

  // Calculate and cache the font metrics.
  void calculateMetrics();

  std::wstring font_name_;
  int font_size_;
  int style_;

  // Cached metrics, generated at construction
  int height_;
  int ascent_;
  int avg_width_;
#endif

};

}  // namespace gfx

#endif  // APP_GFX_FONT_H_
