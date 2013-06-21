# Copyright 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Utilities for performing pixel-by-pixel image comparision."""

import itertools
import StringIO
import PIL
from PIL import Image


def _AreTheSameSize(images):
  """Returns whether a set of images are the size size.

  Args:
    images: a list of images to compare.

  Returns:
    boolean.

  Raises:
    Exception: One image or fewer is passed in.
  """
  if len(images) > 1:
    return all(images[0].size == img.size for img in images[1:])
  else:
    raise Exception('No images passed in.')


def _GetDifferenceWithMask(image1, image2, mask=None,
                           masked_color=(0, 0, 0),
                           same_color=(0, 0, 0),
                           different_color=(255, 255, 255)):
  """Returns an image representing the difference between the two images.

  This function computes the difference between two images taking into
  account a mask if it is provided. The final three arguments represent
  the coloration of the generated image.

  Args:
    image1: the first image to compare.
    image2: the second image to compare.
    mask: an optional mask image consisting of only black and white pixels
      where white pixels indicate the portion of the image to be masked out.
    masked_color: the color of a masked section in the resulting image.
    same_color: the color of an unmasked section that is the same.
      between images 1 and 2 in the resulting image.
    different_color: the color of an unmasked section that is different
      between images 1 and 2 in the resulting image.

  Returns:
    an image repesenting the difference between the two images.

  Raises:
    Exception: if image1, image2, and mask are not the same size.
  """
  image_mask = mask
  if not mask:
    image_mask = PIL.Image.new('RGB', image1.size, (0, 0, 0))
  if not _AreTheSameSize([image1, image2, image_mask]):
    raise Exception('images and mask must be the same size.')
  image_diff = PIL.Image.new('RGB', image1.size, (0, 0, 0))
  data = []
  for m, px1, px2 in itertools.izip(image_mask.getdata(),
                                    image1.getdata(),
                                    image2.getdata()):
    if m == (255, 255, 255):
      data.append(masked_color)
    elif px1 == px2:
      data.append(same_color)
    else:
      data.append(different_color)

  image_diff.putdata(data)
  return image_diff


def CreateMask(images):
  """Computes a mask for a set of images.

  Returns a difference mask that is computed from the images
  which are passed in. The mask will have a white pixel
  anywhere that the input images differ and a black pixel
  everywhere else.

  Args:
    images: the images to compute the mask from.

  Returns:
    an image of only black and white pixels where white pixels represent
      areas in the input images that have differences.

  Raises:
    Exception: if the images passed in are not of the same size.
    Exception: if fewer than two images are passed in.
  """
  if len(images) < 2:
    raise Exception('mask must be created from two or more images.')
  mask = Image.new('RGB', images[0].size, (0, 0, 0))
  image = images[0]
  for other_image in images[1:]:
    mask = _GetDifferenceWithMask(
        image,
        other_image,
        mask,
        masked_color=(255, 255, 255))
  return mask


def VisualizeImageDifferences(image1, image2, mask=None):
  """Returns an image repesenting the unmasked differences between two images.

  Iterates through the pixel values of two images and an optional
  mask. If the pixel values are the same, or the pixel is masked,
  (0,0,0) is stored for that pixel. Otherwise, (255,255,255) is stored.
  This ultimately produces an image where unmasked differences between
  the two images are white pixels, and everything else is black.

  Args:
    image1: an RGB image
    image2: another RGB image of the same size as image1.
    mask: an optional RGB image consisting of only white and black pixels
      where the white pixels represent the parts of the images to be masked
      out.

  Returns:
    a black and white image representing the unmasked difference between
    the two input images.

  Raises:
    Exception: if the two images and optional mask are different sizes.
  """
  return _GetDifferenceWithMask(image1, image2, mask)


def TotalDifferentPixels(image1, image2, mask=None):
  """Computes the number of different pixels between two images.

  Args:
    image1: the first RGB image to be compared.
    image2: the second RGB image to be compared.
    mask: an optional RGB image of only black and white pixels
      where white pixels indicate the parts of the image to be masked out.

  Returns:
    the number of differing pixels between the images.

  Raises:
    Exception: if the images to be compared and the mask are not the same size.
  """
  image_mask = mask
  if not mask:
    image_mask = PIL.Image.new('RGB', image1.size, (0, 0, 0))
  if _AreTheSameSize([image1, image2, image_mask]):
    total_diff = 0
    for px1, px2, m in itertools.izip(image1.getdata(),
                                      image2.getdata(),
                                      image_mask.getdata()):
      if m == (255, 255, 255):
        continue
      elif px1 != px2:
        total_diff += 1
      else:
        continue
    return total_diff
  else:
    raise Exception('images and mask must be the same size')


def SameImage(image1, image2, mask=None):
  """Returns a boolean representing whether the images are the same.

  Returns a boolean indicating whether two images are similar
  enough to be considered the same. Essentially wraps the
  TotalDifferentPixels function.


  Args:
    image1: an RGB image to compare.
    image2: an RGB image to compare.
    mask: an optional image of only black and white pixels
    where white pixels are masked out

  Returns:
    True if the images are similar, False otherwise.

  Raises:
    Exception: if the images (and mask) are different sizes.
  """
  different_pixels = TotalDifferentPixels(image1, image2, mask)
  return different_pixels == 0


def SerializeImage(image):
  """Returns a base64 encoded version of the file-contents of the image.

  Args:
    image: an RGB image to be encoded.

  Returns:
    a base64 encoded string representing the image.
  """
  f = StringIO.StringIO()
  image.save(f, 'PNG')
  encoded_image = f.getvalue().encode('base64')
  f.close()
  return encoded_image


def DeserializeImage(encoded_image):
  """Returns an RGB image from a base64 encoded string.

  Args:
    encoded_image: a base64 encoded string representation of an RGB image.

  Returns:
    an RGB image
  """
  return PIL.Image.open(StringIO.StringIO(encoded_image.decode('base64')))


