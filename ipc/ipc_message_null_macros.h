// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// No include guard, may be included multiple times.

// NULL out all the macros that need NULLing, so that multiple includes of
// the XXXX_messages_internal.h files will not generate noise.
#undef IPC_STRUCT_BEGIN
#undef IPC_STRUCT_MEMBER
#undef IPC_STRUCT_END
#undef IPC_STRUCT_TRAITS_BEGIN
#undef IPC_STRUCT_TRAITS_MEMBER
#undef IPC_STRUCT_TRAITS_PARENT
#undef IPC_STRUCT_TRAITS_END
#undef IPC_ENUM_TRAITS
#undef IPC_MESSAGE_CONTROL0
#undef IPC_MESSAGE_CONTROL1
#undef IPC_MESSAGE_CONTROL2
#undef IPC_MESSAGE_CONTROL3
#undef IPC_MESSAGE_CONTROL4
#undef IPC_MESSAGE_CONTROL5
#undef IPC_MESSAGE_ROUTED0
#undef IPC_MESSAGE_ROUTED1
#undef IPC_MESSAGE_ROUTED2
#undef IPC_MESSAGE_ROUTED3
#undef IPC_MESSAGE_ROUTED4
#undef IPC_MESSAGE_ROUTED5
#undef IPC_SYNC_MESSAGE_CONTROL0_0
#undef IPC_SYNC_MESSAGE_CONTROL0_1
#undef IPC_SYNC_MESSAGE_CONTROL0_2
#undef IPC_SYNC_MESSAGE_CONTROL0_3
#undef IPC_SYNC_MESSAGE_CONTROL0_4
#undef IPC_SYNC_MESSAGE_CONTROL1_0
#undef IPC_SYNC_MESSAGE_CONTROL1_1
#undef IPC_SYNC_MESSAGE_CONTROL1_2
#undef IPC_SYNC_MESSAGE_CONTROL1_3
#undef IPC_SYNC_MESSAGE_CONTROL1_4
#undef IPC_SYNC_MESSAGE_CONTROL2_0
#undef IPC_SYNC_MESSAGE_CONTROL2_1
#undef IPC_SYNC_MESSAGE_CONTROL2_2
#undef IPC_SYNC_MESSAGE_CONTROL2_3
#undef IPC_SYNC_MESSAGE_CONTROL2_4
#undef IPC_SYNC_MESSAGE_CONTROL3_0
#undef IPC_SYNC_MESSAGE_CONTROL3_1
#undef IPC_SYNC_MESSAGE_CONTROL3_2
#undef IPC_SYNC_MESSAGE_CONTROL3_3
#undef IPC_SYNC_MESSAGE_CONTROL3_4
#undef IPC_SYNC_MESSAGE_CONTROL4_0
#undef IPC_SYNC_MESSAGE_CONTROL4_1
#undef IPC_SYNC_MESSAGE_CONTROL4_2
#undef IPC_SYNC_MESSAGE_CONTROL4_3
#undef IPC_SYNC_MESSAGE_CONTROL4_4
#undef IPC_SYNC_MESSAGE_CONTROL5_0
#undef IPC_SYNC_MESSAGE_CONTROL5_1
#undef IPC_SYNC_MESSAGE_CONTROL5_2
#undef IPC_SYNC_MESSAGE_CONTROL5_3
#undef IPC_SYNC_MESSAGE_CONTROL5_4
#undef IPC_SYNC_MESSAGE_ROUTED0_0
#undef IPC_SYNC_MESSAGE_ROUTED0_1
#undef IPC_SYNC_MESSAGE_ROUTED0_2
#undef IPC_SYNC_MESSAGE_ROUTED0_3
#undef IPC_SYNC_MESSAGE_ROUTED0_4
#undef IPC_SYNC_MESSAGE_ROUTED1_0
#undef IPC_SYNC_MESSAGE_ROUTED1_1
#undef IPC_SYNC_MESSAGE_ROUTED1_2
#undef IPC_SYNC_MESSAGE_ROUTED1_3
#undef IPC_SYNC_MESSAGE_ROUTED1_4
#undef IPC_SYNC_MESSAGE_ROUTED2_0
#undef IPC_SYNC_MESSAGE_ROUTED2_1
#undef IPC_SYNC_MESSAGE_ROUTED2_2
#undef IPC_SYNC_MESSAGE_ROUTED2_3
#undef IPC_SYNC_MESSAGE_ROUTED2_4
#undef IPC_SYNC_MESSAGE_ROUTED3_0
#undef IPC_SYNC_MESSAGE_ROUTED3_1
#undef IPC_SYNC_MESSAGE_ROUTED3_2
#undef IPC_SYNC_MESSAGE_ROUTED3_3
#undef IPC_SYNC_MESSAGE_ROUTED3_4
#undef IPC_SYNC_MESSAGE_ROUTED4_0
#undef IPC_SYNC_MESSAGE_ROUTED4_1
#undef IPC_SYNC_MESSAGE_ROUTED4_2
#undef IPC_SYNC_MESSAGE_ROUTED4_3
#undef IPC_SYNC_MESSAGE_ROUTED4_4
#undef IPC_SYNC_MESSAGE_ROUTED5_0
#undef IPC_SYNC_MESSAGE_ROUTED5_1
#undef IPC_SYNC_MESSAGE_ROUTED5_2
#undef IPC_SYNC_MESSAGE_ROUTED5_3
#undef IPC_SYNC_MESSAGE_ROUTED5_4

#define IPC_STRUCT_BEGIN(struct_name)
#define IPC_STRUCT_MEMBER(type, name)
#define IPC_STRUCT_END()
#define IPC_STRUCT_TRAITS_BEGIN(struct_name)
#define IPC_STRUCT_TRAITS_MEMBER(name)
#define IPC_STRUCT_TRAITS_PARENT(type)
#define IPC_STRUCT_TRAITS_END()
#define IPC_ENUM_TRAITS(enum_name)
#define IPC_MESSAGE_CONTROL0(msg_class)
#define IPC_MESSAGE_CONTROL1(msg_class, type1)
#define IPC_MESSAGE_CONTROL2(msg_class, type1, type2)
#define IPC_MESSAGE_CONTROL3(msg_class, type1, type2, type3)
#define IPC_MESSAGE_CONTROL4(msg_class, type1, type2, type3, type4)
#define IPC_MESSAGE_CONTROL5(msg_class, type1, type2, type3, type4, type5)
#define IPC_MESSAGE_ROUTED0(msg_class)
#define IPC_MESSAGE_ROUTED1(msg_class, type1)
#define IPC_MESSAGE_ROUTED2(msg_class, type1, type2)
#define IPC_MESSAGE_ROUTED3(msg_class, type1, type2, type3)
#define IPC_MESSAGE_ROUTED4(msg_class, type1, type2, type3, type4)
#define IPC_MESSAGE_ROUTED5(msg_class, type1, type2, type3, type4, type5)
#define IPC_SYNC_MESSAGE_CONTROL0_0(msg_class)
#define IPC_SYNC_MESSAGE_CONTROL0_1(msg_class, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL0_2(msg_class, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL0_3(msg_class, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL0_4(msg_class, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_CONTROL1_0(msg_class, type1_in)
#define IPC_SYNC_MESSAGE_CONTROL1_1(msg_class, type1_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL1_2(msg_class, type1_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL1_3(msg_class, type1_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL1_4(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_CONTROL2_0(msg_class, type1_in, type2_in)
#define IPC_SYNC_MESSAGE_CONTROL2_1(msg_class, type1_in, type2_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL2_2(msg_class, type1_in, type2_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL2_4(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_CONTROL3_0(msg_class, type1_in, type2_in, type3_in)
#define IPC_SYNC_MESSAGE_CONTROL3_1(msg_class, type1_in, type2_in, type3_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL3_4(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_CONTROL4_0(msg_class, type1_in, type2_in, type3_in, type4_in)
#define IPC_SYNC_MESSAGE_CONTROL4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL4_3(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL4_4(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_CONTROL5_0(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in)
#define IPC_SYNC_MESSAGE_CONTROL5_1(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out)
#define IPC_SYNC_MESSAGE_CONTROL5_2(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_CONTROL5_3(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_CONTROL5_4(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED0_0(msg_class)
#define IPC_SYNC_MESSAGE_ROUTED0_1(msg_class, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED0_2(msg_class, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED0_3(msg_class, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED0_4(msg_class, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED1_0(msg_class, type1_in)
#define IPC_SYNC_MESSAGE_ROUTED1_1(msg_class, type1_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED1_2(msg_class, type1_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED1_3(msg_class, type1_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED1_4(msg_class, type1_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED2_0(msg_class, type1_in, type2_in)
#define IPC_SYNC_MESSAGE_ROUTED2_1(msg_class, type1_in, type2_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED2_2(msg_class, type1_in, type2_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED2_3(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED2_4(msg_class, type1_in, type2_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED3_0(msg_class, type1_in, type2_in, type3_in)
#define IPC_SYNC_MESSAGE_ROUTED3_1(msg_class, type1_in, type2_in, type3_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED3_2(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED3_3(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED3_4(msg_class, type1_in, type2_in, type3_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED4_0(msg_class, type1_in, type2_in, type3_in, type4_in)
#define IPC_SYNC_MESSAGE_ROUTED4_1(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED4_2(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED4_3(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED4_4(msg_class, type1_in, type2_in, type3_in, type4_in, type1_out, type2_out, type3_out, type4_out)
#define IPC_SYNC_MESSAGE_ROUTED5_0(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in)
#define IPC_SYNC_MESSAGE_ROUTED5_1(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out)
#define IPC_SYNC_MESSAGE_ROUTED5_2(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out)
#define IPC_SYNC_MESSAGE_ROUTED5_3(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out)
#define IPC_SYNC_MESSAGE_ROUTED5_4(msg_class, type1_in, type2_in, type3_in, type4_in, type5_in, type1_out, type2_out, type3_out, type4_out)
