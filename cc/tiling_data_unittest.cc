// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiling_data.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

int NumTiles(
    gfx::Size max_texture_size,
    gfx::Size total_size,
    bool has_border_texels) {
  TilingData tiling(max_texture_size, total_size, has_border_texels);
  int num_tiles = tiling.num_tiles_x() * tiling.num_tiles_y();

  // Assert no overflow.
  EXPECT_GE(num_tiles, 0);
  if (num_tiles > 0)
    EXPECT_EQ(num_tiles / tiling.num_tiles_x(), tiling.num_tiles_y());

  return num_tiles;
}

int XIndex(
    gfx::Size max_texture_size,
    gfx::Size total_size,
    bool has_border_texels,
    int x_coord) {
  TilingData tiling(max_texture_size, total_size, has_border_texels);
  return tiling.TileXIndexFromSrcCoord(x_coord);
}

int YIndex(
    gfx::Size max_texture_size,
    gfx::Size total_size,
    bool has_border_texels,
    int y_coord) {
  TilingData tiling(max_texture_size, total_size, has_border_texels);
  return tiling.TileYIndexFromSrcCoord(y_coord);
}

int PosX(
    gfx::Size max_texture_size,
    gfx::Size total_size,
    bool has_border_texels,
    int x_index) {
  TilingData tiling(max_texture_size, total_size, has_border_texels);
  return tiling.TilePositionX(x_index);
}

int PosY(
    gfx::Size max_texture_size,
    gfx::Size total_size,
    bool has_border_texels,
    int y_index) {
  TilingData tiling(max_texture_size, total_size, has_border_texels);
  return tiling.TilePositionY(y_index);
}

int SizeX(
    gfx::Size max_texture_size,
    gfx::Size total_size,
    bool has_border_texels,
    int x_index) {
  TilingData tiling(max_texture_size, total_size, has_border_texels);
  return tiling.TileSizeX(x_index);
}

int SizeY(
    gfx::Size max_texture_size,
    gfx::Size total_size,
    bool has_border_texels,
    int y_index) {
  TilingData tiling(max_texture_size, total_size, has_border_texels);
  return tiling.TileSizeY(y_index);
}

TEST(TilingDataTest, numTiles_NoTiling)
{
  EXPECT_EQ(1, NumTiles(gfx::Size(16, 16), gfx::Size(16, 16), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(16, 16), gfx::Size(15, 15), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(16, 16), gfx::Size(16, 16), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(16, 16), gfx::Size(1, 16), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(15, 15), gfx::Size(15, 15), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(32, 16), gfx::Size(32, 16), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(32, 16), gfx::Size(32, 16), true));
}

TEST(TilingDataTest, numTiles_TilingNoBorders)
{
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), gfx::Size(0, 0), false));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), gfx::Size(4, 0), false));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), gfx::Size(0, 4), false));
  EXPECT_EQ(0, NumTiles(gfx::Size(4, 4), gfx::Size(4, 0), false));
  EXPECT_EQ(0, NumTiles(gfx::Size(4, 4), gfx::Size(0, 4), false));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), gfx::Size(1, 1), false));

  EXPECT_EQ(1, NumTiles(gfx::Size(1, 1), gfx::Size(1, 1), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(1, 1), gfx::Size(1, 2), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(1, 1), gfx::Size(2, 1), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), gfx::Size(1, 1), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), gfx::Size(1, 2), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), gfx::Size(2, 1), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), gfx::Size(2, 2), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(3, 3), gfx::Size(3, 3), false));

  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), gfx::Size(1, 4), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), gfx::Size(2, 4), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), gfx::Size(3, 4), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), gfx::Size(4, 4), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), gfx::Size(5, 4), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), gfx::Size(6, 4), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), gfx::Size(7, 4), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), gfx::Size(8, 4), false));
  EXPECT_EQ(3, NumTiles(gfx::Size(4, 4), gfx::Size(9, 4), false));
  EXPECT_EQ(3, NumTiles(gfx::Size(4, 4), gfx::Size(10, 4), false));
  EXPECT_EQ(3, NumTiles(gfx::Size(4, 4), gfx::Size(11, 4), false));

  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), gfx::Size(1, 5), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), gfx::Size(2, 5), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), gfx::Size(3, 5), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), gfx::Size(4, 5), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), gfx::Size(5, 5), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), gfx::Size(6, 5), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), gfx::Size(7, 5), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), gfx::Size(8, 5), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), gfx::Size(9, 5), false));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), gfx::Size(10, 5), false));
  EXPECT_EQ(3, NumTiles(gfx::Size(5, 5), gfx::Size(11, 5), false));

  EXPECT_EQ(1, NumTiles(gfx::Size(16, 16), gfx::Size(16, 16), false));
  EXPECT_EQ(1, NumTiles(gfx::Size(17, 17), gfx::Size(16, 16), false));
  EXPECT_EQ(4, NumTiles(gfx::Size(15, 15), gfx::Size(16, 16), false));
  EXPECT_EQ(4, NumTiles(gfx::Size(8, 8), gfx::Size(16, 16), false));
  EXPECT_EQ(6, NumTiles(gfx::Size(8, 8), gfx::Size(17, 16), false));

  EXPECT_EQ(8, NumTiles(gfx::Size(5, 8), gfx::Size(17, 16), false));
}

TEST(TilingDataTest, numTiles_TilingWithBorders)
{
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), gfx::Size(0, 0), true));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), gfx::Size(4, 0), true));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), gfx::Size(0, 4), true));
  EXPECT_EQ(0, NumTiles(gfx::Size(4, 4), gfx::Size(4, 0), true));
  EXPECT_EQ(0, NumTiles(gfx::Size(4, 4), gfx::Size(0, 4), true));
  EXPECT_EQ(0, NumTiles(gfx::Size(0, 0), gfx::Size(1, 1), true));

  EXPECT_EQ(1, NumTiles(gfx::Size(1, 1), gfx::Size(1, 1), true));
  EXPECT_EQ(0, NumTiles(gfx::Size(1, 1), gfx::Size(1, 2), true));
  EXPECT_EQ(0, NumTiles(gfx::Size(1, 1), gfx::Size(2, 1), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), gfx::Size(1, 1), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), gfx::Size(1, 2), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), gfx::Size(2, 1), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(2, 2), gfx::Size(2, 2), true));

  EXPECT_EQ(1, NumTiles(gfx::Size(3, 3), gfx::Size(1, 3), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(3, 3), gfx::Size(2, 3), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(3, 3), gfx::Size(3, 3), true));
  EXPECT_EQ(2, NumTiles(gfx::Size(3, 3), gfx::Size(4, 3), true));
  EXPECT_EQ(3, NumTiles(gfx::Size(3, 3), gfx::Size(5, 3), true));
  EXPECT_EQ(4, NumTiles(gfx::Size(3, 3), gfx::Size(6, 3), true));
  EXPECT_EQ(5, NumTiles(gfx::Size(3, 3), gfx::Size(7, 3), true));

  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), gfx::Size(1, 4), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), gfx::Size(2, 4), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), gfx::Size(3, 4), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(4, 4), gfx::Size(4, 4), true));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), gfx::Size(5, 4), true));
  EXPECT_EQ(2, NumTiles(gfx::Size(4, 4), gfx::Size(6, 4), true));
  EXPECT_EQ(3, NumTiles(gfx::Size(4, 4), gfx::Size(7, 4), true));
  EXPECT_EQ(3, NumTiles(gfx::Size(4, 4), gfx::Size(8, 4), true));
  EXPECT_EQ(4, NumTiles(gfx::Size(4, 4), gfx::Size(9, 4), true));
  EXPECT_EQ(4, NumTiles(gfx::Size(4, 4), gfx::Size(10, 4), true));
  EXPECT_EQ(5, NumTiles(gfx::Size(4, 4), gfx::Size(11, 4), true));

  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), gfx::Size(1, 5), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), gfx::Size(2, 5), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), gfx::Size(3, 5), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), gfx::Size(4, 5), true));
  EXPECT_EQ(1, NumTiles(gfx::Size(5, 5), gfx::Size(5, 5), true));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), gfx::Size(6, 5), true));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), gfx::Size(7, 5), true));
  EXPECT_EQ(2, NumTiles(gfx::Size(5, 5), gfx::Size(8, 5), true));
  EXPECT_EQ(3, NumTiles(gfx::Size(5, 5), gfx::Size(9, 5), true));
  EXPECT_EQ(3, NumTiles(gfx::Size(5, 5), gfx::Size(10, 5), true));
  EXPECT_EQ(3, NumTiles(gfx::Size(5, 5), gfx::Size(11, 5), true));

  EXPECT_EQ(30, NumTiles(gfx::Size(8, 5), gfx::Size(16, 32), true));
}

TEST(TilingDataTest, tileXIndexFromSrcCoord)
{
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 0));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 1));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 2));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 3));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 4));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 5));
  EXPECT_EQ(2, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 6));
  EXPECT_EQ(2, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 7));
  EXPECT_EQ(2, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 8));
  EXPECT_EQ(3, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 9));
  EXPECT_EQ(3, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 10));
  EXPECT_EQ(3, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 11));

  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 0));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 1));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 2));
  EXPECT_EQ(2, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 3));
  EXPECT_EQ(3, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 4));
  EXPECT_EQ(4, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 5));
  EXPECT_EQ(5, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 6));
  EXPECT_EQ(6, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 7));
  EXPECT_EQ(7, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 8));
  EXPECT_EQ(7, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 9));
  EXPECT_EQ(7, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 10));
  EXPECT_EQ(7, XIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 11));

  EXPECT_EQ(0, XIndex(gfx::Size(1, 1), gfx::Size(1, 1), false, 0));
  EXPECT_EQ(0, XIndex(gfx::Size(2, 2), gfx::Size(2, 2), false, 0));
  EXPECT_EQ(0, XIndex(gfx::Size(2, 2), gfx::Size(2, 2), false, 1));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(3, 3), false, 0));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(3, 3), false, 1));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(3, 3), false, 2));

  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(4, 3), false, 0));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(4, 3), false, 1));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(4, 3), false, 2));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), gfx::Size(4, 3), false, 3));

  EXPECT_EQ(0, XIndex(gfx::Size(1, 1), gfx::Size(1, 1), true, 0));
  EXPECT_EQ(0, XIndex(gfx::Size(2, 2), gfx::Size(2, 2), true, 0));
  EXPECT_EQ(0, XIndex(gfx::Size(2, 2), gfx::Size(2, 2), true, 1));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(3, 3), true, 0));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(3, 3), true, 1));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(3, 3), true, 2));

  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(4, 3), true, 0));
  EXPECT_EQ(0, XIndex(gfx::Size(3, 3), gfx::Size(4, 3), true, 1));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), gfx::Size(4, 3), true, 2));
  EXPECT_EQ(1, XIndex(gfx::Size(3, 3), gfx::Size(4, 3), true, 3));
}

TEST(TilingDataTest, tileYIndexFromSrcCoord)
{
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 0));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 1));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 2));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 3));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 4));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 5));
  EXPECT_EQ(2, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 6));
  EXPECT_EQ(2, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 7));
  EXPECT_EQ(2, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 8));
  EXPECT_EQ(3, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 9));
  EXPECT_EQ(3, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 10));
  EXPECT_EQ(3, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), false, 11));

  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 0));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 1));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 2));
  EXPECT_EQ(2, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 3));
  EXPECT_EQ(3, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 4));
  EXPECT_EQ(4, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 5));
  EXPECT_EQ(5, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 6));
  EXPECT_EQ(6, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 7));
  EXPECT_EQ(7, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 8));
  EXPECT_EQ(7, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 9));
  EXPECT_EQ(7, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 10));
  EXPECT_EQ(7, YIndex(gfx::Size(3, 3), gfx::Size(10, 10), true, 11));

  EXPECT_EQ(0, YIndex(gfx::Size(1, 1), gfx::Size(1, 1), false, 0));
  EXPECT_EQ(0, YIndex(gfx::Size(2, 2), gfx::Size(2, 2), false, 0));
  EXPECT_EQ(0, YIndex(gfx::Size(2, 2), gfx::Size(2, 2), false, 1));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 3), false, 0));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 3), false, 1));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 3), false, 2));

  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 4), false, 0));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 4), false, 1));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 4), false, 2));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), gfx::Size(3, 4), false, 3));

  EXPECT_EQ(0, YIndex(gfx::Size(1, 1), gfx::Size(1, 1), true, 0));
  EXPECT_EQ(0, YIndex(gfx::Size(2, 2), gfx::Size(2, 2), true, 0));
  EXPECT_EQ(0, YIndex(gfx::Size(2, 2), gfx::Size(2, 2), true, 1));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 3), true, 0));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 3), true, 1));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 3), true, 2));

  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 4), true, 0));
  EXPECT_EQ(0, YIndex(gfx::Size(3, 3), gfx::Size(3, 4), true, 1));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), gfx::Size(3, 4), true, 2));
  EXPECT_EQ(1, YIndex(gfx::Size(3, 3), gfx::Size(3, 4), true, 3));
}

TEST(TilingDataTest, tileSizeX)
{
  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), gfx::Size(5, 5), false, 0));
  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), gfx::Size(5, 5), true, 0));

  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), gfx::Size(6, 6), false, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(5, 5), gfx::Size(6, 6), false, 1));
  EXPECT_EQ(4, SizeX(gfx::Size(5, 5), gfx::Size(6, 6), true, 0));
  EXPECT_EQ(2, SizeX(gfx::Size(5, 5), gfx::Size(6, 6), true, 1));

  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), gfx::Size(8, 8), false, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(5, 5), gfx::Size(8, 8), false, 1));
  EXPECT_EQ(4, SizeX(gfx::Size(5, 5), gfx::Size(8, 8), true, 0));
  EXPECT_EQ(4, SizeX(gfx::Size(5, 5), gfx::Size(8, 8), true, 1));

  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), gfx::Size(10, 10), false, 0));
  EXPECT_EQ(5, SizeX(gfx::Size(5, 5), gfx::Size(10, 10), false, 1));
  EXPECT_EQ(4, SizeX(gfx::Size(5, 5), gfx::Size(10, 10), true, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(5, 5), gfx::Size(10, 10), true, 1));
  EXPECT_EQ(3, SizeX(gfx::Size(5, 5), gfx::Size(10, 10), true, 2));

  EXPECT_EQ(4, SizeX(gfx::Size(5, 5), gfx::Size(11, 11), true, 2));
  EXPECT_EQ(3, SizeX(gfx::Size(5, 5), gfx::Size(12, 12), true, 2));

  EXPECT_EQ(3, SizeX(gfx::Size(5, 9), gfx::Size(12, 17), true, 2));
}

TEST(TilingDataTest, TileSizeY)
{
  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), gfx::Size(5, 5), false, 0));
  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), gfx::Size(5, 5), true, 0));

  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), gfx::Size(6, 6), false, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(5, 5), gfx::Size(6, 6), false, 1));
  EXPECT_EQ(4, SizeY(gfx::Size(5, 5), gfx::Size(6, 6), true, 0));
  EXPECT_EQ(2, SizeY(gfx::Size(5, 5), gfx::Size(6, 6), true, 1));

  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), gfx::Size(8, 8), false, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(5, 5), gfx::Size(8, 8), false, 1));
  EXPECT_EQ(4, SizeY(gfx::Size(5, 5), gfx::Size(8, 8), true, 0));
  EXPECT_EQ(4, SizeY(gfx::Size(5, 5), gfx::Size(8, 8), true, 1));

  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), gfx::Size(10, 10), false, 0));
  EXPECT_EQ(5, SizeY(gfx::Size(5, 5), gfx::Size(10, 10), false, 1));
  EXPECT_EQ(4, SizeY(gfx::Size(5, 5), gfx::Size(10, 10), true, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(5, 5), gfx::Size(10, 10), true, 1));
  EXPECT_EQ(3, SizeY(gfx::Size(5, 5), gfx::Size(10, 10), true, 2));

  EXPECT_EQ(4, SizeY(gfx::Size(5, 5), gfx::Size(11, 11), true, 2));
  EXPECT_EQ(3, SizeY(gfx::Size(5, 5), gfx::Size(12, 12), true, 2));

  EXPECT_EQ(3, SizeY(gfx::Size(9, 5), gfx::Size(17, 12), true, 2));
}

TEST(TilingDataTest, TileSizeX_and_TilePositionX)
{
  // Single tile cases:
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), gfx::Size(1, 1), false, 0));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(1, 1), false, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), gfx::Size(1, 100), false, 0));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(1, 100), false, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), gfx::Size(3, 1), false, 0));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(3, 1), false, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), gfx::Size(3, 100), false, 0));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(3, 100), false, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), gfx::Size(1, 1), true, 0));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(1, 1), true, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), gfx::Size(1, 100), true, 0));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(1, 100), true, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), gfx::Size(3, 1), true, 0));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(3, 1), true, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), gfx::Size(3, 100), true, 0));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(3, 100), true, 0));

  // Multiple tiles:
  // no border
  // positions 0, 3
  EXPECT_EQ(2, NumTiles(gfx::Size(3, 3), gfx::Size(6, 1), false));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), gfx::Size(6, 1), false, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), gfx::Size(6, 1), false, 1));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(6, 1), false, 0));
  EXPECT_EQ(3, PosX(gfx::Size(3, 3), gfx::Size(6, 1), false, 1));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), gfx::Size(6, 100), false, 0));
  EXPECT_EQ(3, SizeX(gfx::Size(3, 3), gfx::Size(6, 100), false, 1));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(6, 100), false, 0));
  EXPECT_EQ(3, PosX(gfx::Size(3, 3), gfx::Size(6, 100), false, 1));

  // Multiple tiles:
  // with border
  // positions 0, 2, 3, 4
  EXPECT_EQ(4, NumTiles(gfx::Size(3, 3), gfx::Size(6, 1), true));
  EXPECT_EQ(2, SizeX(gfx::Size(3, 3), gfx::Size(6, 1), true, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), gfx::Size(6, 1), true, 1));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 3), gfx::Size(6, 1), true, 2));
  EXPECT_EQ(2, SizeX(gfx::Size(3, 3), gfx::Size(6, 1), true, 3));
  EXPECT_EQ(0, PosX(gfx::Size(3, 3), gfx::Size(6, 1), true, 0));
  EXPECT_EQ(2, PosX(gfx::Size(3, 3), gfx::Size(6, 1), true, 1));
  EXPECT_EQ(3, PosX(gfx::Size(3, 3), gfx::Size(6, 1), true, 2));
  EXPECT_EQ(4, PosX(gfx::Size(3, 3), gfx::Size(6, 1), true, 3));
  EXPECT_EQ(2, SizeX(gfx::Size(3, 7), gfx::Size(6, 100), true, 0));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 7), gfx::Size(6, 100), true, 1));
  EXPECT_EQ(1, SizeX(gfx::Size(3, 7), gfx::Size(6, 100), true, 2));
  EXPECT_EQ(2, SizeX(gfx::Size(3, 7), gfx::Size(6, 100), true, 3));
  EXPECT_EQ(0, PosX(gfx::Size(3, 7), gfx::Size(6, 100), true, 0));
  EXPECT_EQ(2, PosX(gfx::Size(3, 7), gfx::Size(6, 100), true, 1));
  EXPECT_EQ(3, PosX(gfx::Size(3, 7), gfx::Size(6, 100), true, 2));
  EXPECT_EQ(4, PosX(gfx::Size(3, 7), gfx::Size(6, 100), true, 3));
}

TEST(TilingDataTest, TileSizeY_and_TilePositionY)
{
  // Single tile cases:
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), gfx::Size(1, 1), false, 0));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(1, 1), false, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), gfx::Size(100, 1), false, 0));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(100, 1), false, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), gfx::Size(1, 3), false, 0));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(1, 3), false, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), gfx::Size(100, 3), false, 0));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(100, 3), false, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), gfx::Size(1, 1), true, 0));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(1, 1), true, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), gfx::Size(100, 1), true, 0));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(100, 1), true, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), gfx::Size(1, 3), true, 0));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(1, 3), true, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), gfx::Size(100, 3), true, 0));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(100, 3), true, 0));

  // Multiple tiles:
  // no border
  // positions 0, 3
  EXPECT_EQ(2, NumTiles(gfx::Size(3, 3), gfx::Size(1, 6), false));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), gfx::Size(1, 6), false, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), gfx::Size(1, 6), false, 1));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(1, 6), false, 0));
  EXPECT_EQ(3, PosY(gfx::Size(3, 3), gfx::Size(1, 6), false, 1));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), gfx::Size(100, 6), false, 0));
  EXPECT_EQ(3, SizeY(gfx::Size(3, 3), gfx::Size(100, 6), false, 1));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(100, 6), false, 0));
  EXPECT_EQ(3, PosY(gfx::Size(3, 3), gfx::Size(100, 6), false, 1));

  // Multiple tiles:
  // with border
  // positions 0, 2, 3, 4
  EXPECT_EQ(4, NumTiles(gfx::Size(3, 3), gfx::Size(1, 6), true));
  EXPECT_EQ(2, SizeY(gfx::Size(3, 3), gfx::Size(1, 6), true, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), gfx::Size(1, 6), true, 1));
  EXPECT_EQ(1, SizeY(gfx::Size(3, 3), gfx::Size(1, 6), true, 2));
  EXPECT_EQ(2, SizeY(gfx::Size(3, 3), gfx::Size(1, 6), true, 3));
  EXPECT_EQ(0, PosY(gfx::Size(3, 3), gfx::Size(1, 6), true, 0));
  EXPECT_EQ(2, PosY(gfx::Size(3, 3), gfx::Size(1, 6), true, 1));
  EXPECT_EQ(3, PosY(gfx::Size(3, 3), gfx::Size(1, 6), true, 2));
  EXPECT_EQ(4, PosY(gfx::Size(3, 3), gfx::Size(1, 6), true, 3));
  EXPECT_EQ(2, SizeY(gfx::Size(7, 3), gfx::Size(100, 6), true, 0));
  EXPECT_EQ(1, SizeY(gfx::Size(7, 3), gfx::Size(100, 6), true, 1));
  EXPECT_EQ(1, SizeY(gfx::Size(7, 3), gfx::Size(100, 6), true, 2));
  EXPECT_EQ(2, SizeY(gfx::Size(7, 3), gfx::Size(100, 6), true, 3));
  EXPECT_EQ(0, PosY(gfx::Size(7, 3), gfx::Size(100, 6), true, 0));
  EXPECT_EQ(2, PosY(gfx::Size(7, 3), gfx::Size(100, 6), true, 1));
  EXPECT_EQ(3, PosY(gfx::Size(7, 3), gfx::Size(100, 6), true, 2));
  EXPECT_EQ(4, PosY(gfx::Size(7, 3), gfx::Size(100, 6), true, 3));
}

TEST(TilingDataTest, SetTotalSize)
{
  TilingData data(gfx::Size(5, 5), gfx::Size(5, 5), false);
  EXPECT_EQ(5, data.total_size().width());
  EXPECT_EQ(5, data.total_size().height());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(5, data.TileSizeX(0));
  EXPECT_EQ(1, data.num_tiles_y());
  EXPECT_EQ(5, data.TileSizeY(0));

  data.SetTotalSize(gfx::Size(6, 5));
  EXPECT_EQ(6, data.total_size().width());
  EXPECT_EQ(5, data.total_size().height());
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(5, data.TileSizeX(0));
  EXPECT_EQ(1, data.TileSizeX(1));
  EXPECT_EQ(1, data.num_tiles_y());
  EXPECT_EQ(5, data.TileSizeY(0));

  data.SetTotalSize(gfx::Size(5, 12));
  EXPECT_EQ(5, data.total_size().width());
  EXPECT_EQ(12, data.total_size().height());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(5, data.TileSizeX(0));
  EXPECT_EQ(3, data.num_tiles_y());
  EXPECT_EQ(5, data.TileSizeY(0));
  EXPECT_EQ(5, data.TileSizeY(1));
  EXPECT_EQ(2, data.TileSizeY(2));
}

TEST(TilingDataTest, SetMaxTextureSizeNoBorders)
{
  TilingData data(gfx::Size(8, 8), gfx::Size(16, 32), false);
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(4, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(32, 32));
  EXPECT_EQ(gfx::Size(32, 32), data.max_texture_size());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(1, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(2, 2));
  EXPECT_EQ(gfx::Size(2, 2), data.max_texture_size());
  EXPECT_EQ(8, data.num_tiles_x());
  EXPECT_EQ(16, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(5, 5));
  EXPECT_EQ(gfx::Size(5, 5), data.max_texture_size());
  EXPECT_EQ(4, data.num_tiles_x());
  EXPECT_EQ(7, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(8, 5));
  EXPECT_EQ(gfx::Size(8, 5), data.max_texture_size());
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(7, data.num_tiles_y());
}

TEST(TilingDataTest, SetMaxTextureSizeBorders)
{
  TilingData data(gfx::Size(8, 8), gfx::Size(16, 32), true);
  EXPECT_EQ(3, data.num_tiles_x());
  EXPECT_EQ(5, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(32, 32));
  EXPECT_EQ(gfx::Size(32, 32), data.max_texture_size());
  EXPECT_EQ(1, data.num_tiles_x());
  EXPECT_EQ(1, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(2, 2));
  EXPECT_EQ(gfx::Size(2, 2), data.max_texture_size());
  EXPECT_EQ(0, data.num_tiles_x());
  EXPECT_EQ(0, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(5, 5));
  EXPECT_EQ(gfx::Size(5, 5), data.max_texture_size());
  EXPECT_EQ(5, data.num_tiles_x());
  EXPECT_EQ(10, data.num_tiles_y());

  data.SetMaxTextureSize(gfx::Size(8, 5));
  EXPECT_EQ(gfx::Size(8, 5), data.max_texture_size());
  EXPECT_EQ(3, data.num_tiles_x());
  EXPECT_EQ(10, data.num_tiles_y());
}

TEST(TilingDataTest, assignment)
{
  {
    TilingData source(gfx::Size(8, 8), gfx::Size(16, 32), true);
    TilingData dest = source;
    EXPECT_EQ(source.border_texels(), dest.border_texels());
    EXPECT_EQ(source.max_texture_size(), dest.max_texture_size());
    EXPECT_EQ(source.num_tiles_x(), dest.num_tiles_x());
    EXPECT_EQ(source.num_tiles_y(), dest.num_tiles_y());
    EXPECT_EQ(source.total_size().width(), dest.total_size().width());
    EXPECT_EQ(source.total_size().height(), dest.total_size().height());
  }
  {
    TilingData source(gfx::Size(7, 3), gfx::Size(6, 100), false);
    TilingData dest(source);
    EXPECT_EQ(source.border_texels(), dest.border_texels());
    EXPECT_EQ(source.max_texture_size(), dest.max_texture_size());
    EXPECT_EQ(source.num_tiles_x(), dest.num_tiles_x());
    EXPECT_EQ(source.num_tiles_y(), dest.num_tiles_y());
    EXPECT_EQ(source.total_size().width(), dest.total_size().width());
    EXPECT_EQ(source.total_size().height(), dest.total_size().height());
  }
}

TEST(TilingDataTest, setBorderTexels)
{
  TilingData data(gfx::Size(8, 8), gfx::Size(16, 32), false);
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(4, data.num_tiles_y());

  data.SetHasBorderTexels(true);
  EXPECT_EQ(3, data.num_tiles_x());
  EXPECT_EQ(5, data.num_tiles_y());

  data.SetHasBorderTexels(true);
  EXPECT_EQ(3, data.num_tiles_x());
  EXPECT_EQ(5, data.num_tiles_y());

  data.SetHasBorderTexels(false);
  EXPECT_EQ(2, data.num_tiles_x());
  EXPECT_EQ(4, data.num_tiles_y());
}

}  // namespace
}  // namespace cc
