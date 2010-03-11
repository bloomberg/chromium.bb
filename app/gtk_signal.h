// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GTK_SIGNAL_H_
#define APP_GTK_SIGNAL_H_

typedef void* gpointer;
typedef struct _GtkWidget GtkWidget;

// At the time of writing this, there were two common ways of binding our C++
// code to the gobject C system. We either defined a whole bunch of "static
// MethodThunk()" which just called nonstatic Method()s on a class (which hurt
// readability of the headers and signal connection code) OR we declared
// "static Method()" and passed in the current object as the gpointer (and hurt
// readability in the implementation by having "context->" before every
// variable).

// The hopeful result of using these macros is that the code will be more
// readable and regular. There shouldn't be a bunch of static Thunks visible in
// the headers and the implementations shouldn't be filled with "context->"
// de-references.

#define CHROMEGTK_CALLBACK_0(TYPE, RETURN, METHOD)                      \
  static RETURN METHOD ## Thunk(GtkWidget* widget, gpointer userdata) { \
    return reinterpret_cast<TYPE*>(userdata)->METHOD(widget);           \
  }                                                                     \
                                                                        \
  RETURN METHOD(GtkWidget* widget);

#define CHROMEGTK_CALLBACK_1(TYPE, RETURN, METHOD, ARG1)            \
  static RETURN METHOD ## Thunk(GtkWidget* widget, ARG1 one,        \
                                gpointer userdata) {                \
    return reinterpret_cast<TYPE*>(userdata)->METHOD(widget, one);  \
  }                                                                 \
                                                                    \
  RETURN METHOD(GtkWidget* widget, ARG1 one);

#define CHROMEGTK_CALLBACK_2(TYPE, RETURN, METHOD, ARG1, ARG2)          \
  static RETURN METHOD ## Thunk(GtkWidget* widget, ARG1 one, ARG2 two,  \
                                gpointer userdata) {                    \
    return reinterpret_cast<TYPE*>(userdata)->METHOD(widget, one, two); \
  }                                                                     \
                                                                        \
  RETURN METHOD(GtkWidget* widget, ARG1 one, ARG2 two);

#define CHROMEGTK_CALLBACK_3(TYPE, RETURN, METHOD, ARG1, ARG2, ARG3)    \
  static RETURN METHOD ## Thunk(GtkWidget* widget, ARG1 one, ARG2 two,  \
                                ARG3 three, gpointer userdata) {        \
    return reinterpret_cast<TYPE*>(userdata)->                          \
        METHOD(widget, one, two, three);                                \
  }                                                                     \
                                                                        \
  RETURN METHOD(GtkWidget* widget, ARG1 one, ARG2 two, ARG3 three);

#define CHROMEGTK_CALLBACK_4(TYPE, RETURN, METHOD, ARG1, ARG2, ARG3, ARG4) \
  static RETURN METHOD ## Thunk(GtkWidget* widget, ARG1 one, ARG2 two,  \
                                ARG3 three, ARG4 four,                  \
                                gpointer userdata) {                    \
    return reinterpret_cast<TYPE*>(userdata)->                          \
        METHOD(widget, one, two, three, four);                          \
  }                                                                     \
                                                                        \
  RETURN METHOD(GtkWidget* widget, ARG1 one, ARG2 two, ARG3 three,      \
                ARG4 four);

#define CHROMEGTK_CALLBACK_5(TYPE, RETURN, METHOD, ARG1, ARG2, ARG3, ARG4, \
                             ARG5)                                      \
  static RETURN METHOD ## Thunk(GtkWidget* widget, ARG1 one, ARG2 two,  \
                                ARG3 three, ARG4 four, ARG5 five,       \
                                gpointer userdata) {                    \
    return reinterpret_cast<TYPE*>(userdata)->                          \
        METHOD(widget, one, two, three, four, five);                    \
  }                                                                     \
                                                                        \
  RETURN METHOD(GtkWidget* widget, ARG1 one, ARG2 two, ARG3 three,      \
                ARG4 four, ARG5 five);

#define CHROMEGTK_CALLBACK_6(TYPE, RETURN, METHOD, ARG1, ARG2, ARG3, ARG4, \
                             ARG5, ARG6)                                \
  static RETURN METHOD ## Thunk(GtkWidget* widget, ARG1 one, ARG2 two,  \
                                ARG3 three, ARG4 four, ARG5 five,       \
                                ARG6 six, gpointer userdata) {          \
    return reinterpret_cast<TYPE*>(userdata)->                          \
        METHOD(widget, one, two, three, four, five, six);               \
  }                                                                     \
                                                                        \
  RETURN METHOD(GtkWidget* widget, ARG1 one, ARG2 two, ARG3 three,      \
                ARG4 four, ARG5 five, ARG6 six);

#endif  // APP_GTK_SIGNAL_H_
