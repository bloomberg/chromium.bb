// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_NOTIFIER_BASE_SIGSLOTREPEATER_H_
#define JINGLE_NOTIFIER_BASE_SIGSLOTREPEATER_H_

// Repeaters are both signals and slots, which are designed as intermediate
// pass-throughs for signals and slots which don't know about each other (for
// modularity or encapsulation).  This eliminates the need to declare a signal
// handler whose sole purpose is to fire another signal.  The repeater connects
// to the originating signal using the 'repeat' method.  When the repeated
// signal fires, the repeater will also fire.

#include "talk/base/sigslot.h"

namespace sigslot {

template<class mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
class repeater0 : public signal0<mt_policy>,
                  public has_slots<mt_policy> {
 public:
  typedef signal0<mt_policy> base_type;
  typedef repeater0<mt_policy> this_type;

  repeater0() { }
  explicit repeater0(const this_type& s) : base_type(s) { }

  void reemit() { signal0<mt_policy>::emit(); }
  void repeat(base_type &s) { s.connect(this, &this_type::reemit); }
};

template<class arg1_type, class mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
class repeater1 : public signal1<arg1_type, mt_policy>,
                  public has_slots<mt_policy> {
 public:
  typedef signal1<arg1_type, mt_policy> base_type;
  typedef repeater1<arg1_type, mt_policy> this_type;

  repeater1() { }
  repeater1(const this_type& s) : base_type(s) { }

  void reemit(arg1_type a1) { signal1<arg1_type, mt_policy>::emit(a1); }
  void repeat(base_type& s) { s.connect(this, &this_type::reemit); }
};

template<class arg1_type, class arg2_type,
         class mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
class repeater2 : public signal2<arg1_type, arg2_type, mt_policy>,
                  public has_slots<mt_policy> {
 public:
  typedef signal2<arg1_type, arg2_type, mt_policy> base_type;
  typedef repeater2<arg1_type, arg2_type, mt_policy> this_type;

  repeater2() { }
  repeater2(const this_type& s) : base_type(s) { }

  void reemit(arg1_type a1, arg2_type a2) {
    signal2<arg1_type, arg2_type, mt_policy>::emit(a1, a2);
  }
  void repeat(base_type& s) { s.connect(this, &this_type::reemit); }
};

template<class arg1_type, class arg2_type, class arg3_type,
         class mt_policy = SIGSLOT_DEFAULT_MT_POLICY>
class repeater3 : public signal3<arg1_type, arg2_type, arg3_type, mt_policy>,
                  public has_slots<mt_policy> {
 public:
  typedef signal3<arg1_type, arg2_type, arg3_type, mt_policy> base_type;
  typedef repeater3<arg1_type, arg2_type, arg3_type, mt_policy> this_type;

  repeater3() { }
  repeater3(const this_type& s) : base_type(s) { }

  void reemit(arg1_type a1, arg2_type a2, arg3_type a3) {
    signal3<arg1_type, arg2_type, arg3_type, mt_policy>::emit(a1, a2, a3);
  }
  void repeat(base_type& s) { s.connect(this, &this_type::reemit); }
};

}  // namespace sigslot

#endif  // JINGLE_NOTIFIER_BASE_SIGSLOTREPEATER_H_
