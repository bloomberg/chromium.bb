/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "gtest/gtest.h"

#include "nacl_io/fifo_char.h"
#include "nacl_io/fifo_null.h"
#include "nacl_io/fifo_packet.h"
#include "nacl_io/packet.h"

#include "ppapi_simple/ps.h"

using namespace nacl_io;

namespace {
const size_t kTestSize = 32;
const size_t kHalfSize = 16;
const size_t kQuarterSize = 8;
};

TEST(FIFONull, Basic) {
  FIFONull fifo;
  EXPECT_FALSE(fifo.IsFull());
  EXPECT_FALSE(fifo.IsEmpty());

  EXPECT_LT(0, fifo.ReadAvailable());
  EXPECT_LT(0, fifo.WriteAvailable());
}

TEST(FIFOChar, Wrap) {
  char temp_wr[kTestSize * 2];
  char temp_rd[kTestSize * 2];
  size_t wr_offs = 0;
  size_t rd_offs = 0;

  FIFOChar fifo(kTestSize);

  memset(temp_rd, 0, sizeof(temp_rd));
  for (size_t index = 0; index < sizeof(temp_wr); index++)
    temp_wr[index] = index;

  EXPECT_TRUE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  // Wrap read and write differently, and verify copy is correct
  EXPECT_EQ(0, fifo.ReadAvailable());
  EXPECT_EQ(kTestSize, fifo.WriteAvailable());

  wr_offs += fifo.Write(temp_wr, kHalfSize);
  EXPECT_EQ(kHalfSize, wr_offs);

  EXPECT_FALSE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  rd_offs += fifo.Read(temp_rd, kQuarterSize);
  EXPECT_EQ(kQuarterSize, rd_offs);

  EXPECT_FALSE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  wr_offs += fifo.Write(&temp_wr[wr_offs], kTestSize);
  EXPECT_EQ(kTestSize + kQuarterSize, wr_offs);

  EXPECT_FALSE(fifo.IsEmpty());

  rd_offs += fifo.Read(&temp_rd[rd_offs], kTestSize);
  EXPECT_EQ(kTestSize + kQuarterSize, rd_offs);

  EXPECT_TRUE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  for (size_t index = 0; index < kQuarterSize + kTestSize; index++)
    EXPECT_EQ((char) index, temp_rd[index]);
}

TEST(FIFOPacket, Packets) {
  char temp_wr[kTestSize];
  FIFOPacket fifo(kTestSize);

  Packet* pkt0 = new Packet(NULL);
  Packet* pkt1 = new Packet(NULL);
  pkt0->Copy(temp_wr, kHalfSize, 0);
  pkt1->Copy(temp_wr, kTestSize, 0);

  EXPECT_TRUE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  EXPECT_EQ(0, fifo.ReadAvailable());
  EXPECT_EQ(kTestSize, fifo.WriteAvailable());

  fifo.WritePacket(pkt0);
  EXPECT_FALSE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  EXPECT_EQ(kHalfSize, fifo.ReadAvailable());
  EXPECT_EQ(kHalfSize, fifo.WriteAvailable());

  fifo.WritePacket(pkt1);
  EXPECT_FALSE(fifo.IsEmpty());
  EXPECT_TRUE(fifo.IsFull());

  EXPECT_EQ(kHalfSize + kTestSize, fifo.ReadAvailable());
  EXPECT_EQ(0, fifo.WriteAvailable());

  EXPECT_EQ(pkt0, fifo.ReadPacket());
  EXPECT_FALSE(fifo.IsEmpty());
  EXPECT_TRUE(fifo.IsFull());

  EXPECT_EQ(kTestSize, fifo.ReadAvailable());
  EXPECT_EQ(0, fifo.WriteAvailable());

  EXPECT_EQ(pkt1, fifo.ReadPacket());

  EXPECT_TRUE(fifo.IsEmpty());
  EXPECT_FALSE(fifo.IsFull());

  EXPECT_EQ(0, fifo.ReadAvailable());
  EXPECT_EQ(kTestSize, fifo.WriteAvailable());

}
