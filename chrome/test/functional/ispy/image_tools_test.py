# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import unittest
from PIL import Image

import image_tools


def _GenImage(size, color):
  return Image.new('RGB', size, color)


def _AllPixelsOfColor(image, color):
  return not any(px != color for px in image.getdata())


class ImageToolsTest(unittest.TestCase):

  def testAreTheSameSize(self):
    black25x25 = _GenImage((25, 25), (0, 0, 0))
    white25x25 = _GenImage((25, 25), (255, 255, 255))
    black50x50 = _GenImage((50, 50), (0, 0, 0))
    white50x50 = _GenImage((50, 50), (255, 255, 255))

    self.assertTrue(image_tools._AreTheSameSize([black25x25, black25x25]))
    self.assertTrue(image_tools._AreTheSameSize([white25x25, white25x25]))
    self.assertTrue(image_tools._AreTheSameSize([black50x50, black50x50]))
    self.assertTrue(image_tools._AreTheSameSize([white50x50, white50x50]))
    self.assertTrue(image_tools._AreTheSameSize([black25x25, white25x25]))
    self.assertTrue(image_tools._AreTheSameSize([black50x50, white50x50]))

    self.assertFalse(image_tools._AreTheSameSize([black50x50, black25x25]))
    self.assertFalse(image_tools._AreTheSameSize([white50x50, white25x25]))
    self.assertFalse(image_tools._AreTheSameSize([black25x25, white50x50]))
    self.assertFalse(image_tools._AreTheSameSize([black50x50, white25x25]))

    self.assertRaises(Exception, image_tools._AreTheSameSize, [])
    self.assertRaises(Exception, image_tools._AreTheSameSize, [black50x50])

  def testGetDifferenceWithMask(self):
    black25x25 = _GenImage((25, 25), (0, 0, 0))
    white25x25 = _GenImage((25, 25), (255, 255, 255))
    black50x50 = _GenImage((50, 50), (0, 0, 0))
    self.assertTrue(_AllPixelsOfColor(image_tools._GetDifferenceWithMask(
        black25x25, black25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools._GetDifferenceWithMask(
        white25x25, white25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools._GetDifferenceWithMask(
        white25x25, black25x25), (255, 255, 255)))
    self.assertTrue(_AllPixelsOfColor(image_tools._GetDifferenceWithMask(
        black25x25, black25x25, mask=black25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools._GetDifferenceWithMask(
        black25x25, black25x25, mask=white25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools._GetDifferenceWithMask(
        black25x25, white25x25, mask=black25x25), (255, 255, 255)))
    self.assertTrue(_AllPixelsOfColor(image_tools._GetDifferenceWithMask(
        black25x25, white25x25, mask=white25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools._GetDifferenceWithMask(
        black25x25, white25x25, different_color=(255, 0, 0)), (255, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools._GetDifferenceWithMask(
        white25x25, white25x25, same_color=(255, 0, 0)), (255, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools._GetDifferenceWithMask(
        white25x25, white25x25,
        mask=white25x25, masked_color=(255, 0, 0)), (255, 0, 0)))
    self.assertRaises(Exception, image_tools._GetDifferenceWithMask,
                      white25x25,
                      black50x50)
    self.assertRaises(Exception, image_tools._GetDifferenceWithMask,
                      white25x25,
                      white25x25,
                      mask=black50x50)

  def testCreateMask(self):
    black25x25 = _GenImage((25, 25), (0, 0, 0))
    white25x25 = _GenImage((25, 25), (255, 255, 255))

    m1 = image_tools.CreateMask([black25x25, white25x25])
    self.assertTrue(_AllPixelsOfColor(m1, (255, 255, 255)))
    m2 = image_tools.CreateMask([black25x25, black25x25])
    self.assertTrue(_AllPixelsOfColor(m2, (0, 0, 0)))
    m3 = image_tools.CreateMask([white25x25, white25x25])
    self.assertTrue(_AllPixelsOfColor(m3, (0, 0, 0)))

  def testTotalDifferentPixels(self):
    white25x25 = _GenImage((25, 25), (255, 255, 255))
    white50x50 = _GenImage((50, 50), (255, 255, 255))
    black25x25 = _GenImage((25, 25), (0, 0, 0))

    self.assertEquals(image_tools.TotalDifferentPixels(white25x25,
                                                       white25x25),
                      0)
    self.assertEquals(image_tools.TotalDifferentPixels(black25x25,
                                                       black25x25),
                      0)
    self.assertEquals(image_tools.TotalDifferentPixels(white25x25,
                                                       black25x25),
                      25*25)
    self.assertEquals(image_tools.TotalDifferentPixels(white25x25,
                                                       black25x25,
                                                       mask=white25x25),
                      0)
    self.assertEquals(image_tools.TotalDifferentPixels(white25x25,
                                                       white25x25,
                                                       mask=white25x25),
                      0)
    self.assertEquals(image_tools.TotalDifferentPixels(white25x25,
                                                       black25x25,
                                                       mask=black25x25),
                      25*25)
    self.assertEquals(image_tools.TotalDifferentPixels(white25x25,
                                                       white25x25,
                                                       mask=black25x25),
                      0)
    self.assertRaises(Exception, image_tools.TotalDifferentPixels,
                      white25x25, white50x50)
    self.assertRaises(Exception, image_tools.TotalDifferentPixels,
                      white25x25, white25x25, mask=white50x50)

  def testSameImage(self):
    white25x25 = _GenImage((25, 25), (255, 255, 255))
    black25x25 = _GenImage((25, 25), (0, 0, 0))
    white50x50 = _GenImage((50, 50), (255, 255, 255))

    self.assertTrue(image_tools.SameImage(white25x25, white25x25))
    self.assertFalse(image_tools.SameImage(white25x25, black25x25))

    self.assertTrue(image_tools.SameImage(white25x25, black25x25,
                                          mask=white25x25))
    self.assertFalse(image_tools.SameImage(white25x25, black25x25,
                                           mask=black25x25))
    self.assertTrue(image_tools.SameImage(black25x25, black25x25))
    self.assertTrue(image_tools.SameImage(black25x25, black25x25,
                                          mask=white25x25))
    self.assertTrue(image_tools.SameImage(white25x25, white25x25,
                                          mask=white25x25))
    self.assertRaises(Exception, image_tools.SameImage,
                      white25x25, white50x50)
    self.assertRaises(Exception, image_tools.SameImage,
                      white25x25, white25x25,
                      mask=white50x50)

  def testVisualizeImageDifferences(self):
    white25x25 = _GenImage((25, 25), (255, 255, 255))
    black25x25 = _GenImage((25, 25), (0, 0, 0))
    white50x50 = _GenImage((50, 50), (255, 255, 255))
    self.assertTrue(_AllPixelsOfColor(image_tools.VisualizeImageDifferences(
        white25x25, black25x25), (255, 255, 255)))
    self.assertTrue(_AllPixelsOfColor(image_tools.VisualizeImageDifferences(
        white25x25, white25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools.VisualizeImageDifferences(
        black25x25, black25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools.VisualizeImageDifferences(
        white25x25, black25x25, mask=white25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools.VisualizeImageDifferences(
        white25x25, black25x25, mask=black25x25), (255, 255, 255)))
    self.assertTrue(_AllPixelsOfColor(image_tools.VisualizeImageDifferences(
        white25x25, white25x25, mask=white25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools.VisualizeImageDifferences(
        white25x25, white25x25, mask=black25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools.VisualizeImageDifferences(
        black25x25, black25x25, mask=black25x25), (0, 0, 0)))
    self.assertTrue(_AllPixelsOfColor(image_tools.VisualizeImageDifferences(
        black25x25, black25x25, mask=white25x25), (0, 0, 0)))

    self.assertRaises(Exception,
                      image_tools.VisualizeImageDifferences,
                      white25x25,
                      white50x50)
    self.assertRaises(Exception,
                      image_tools.VisualizeImageDifferences,
                      white25x25,
                      black25x25,
                      mask=white50x50)

  def testSerialization(self):
    black25x25 = _GenImage((25, 25), (0, 0, 0))
    white25x25 = _GenImage((25, 25), (255, 255, 255))
    self.assertTrue(_AllPixelsOfColor(
        image_tools.DeserializeImage(
            image_tools.SerializeImage(white25x25)), (255, 255, 255)))
    self.assertTrue(_AllPixelsOfColor(
        image_tools.DeserializeImage(
            image_tools.SerializeImage(black25x25)), (0, 0, 0)))


if __name__ == '__main__':
  unittest.main()
