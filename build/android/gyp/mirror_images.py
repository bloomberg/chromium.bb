#!/usr/bin/env python
#
# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Mirrors (i.e. horizontally flips) images in the Android res folder for use
in right-to-left (RTL) mode on Android.

Only some images are mirrored, as determined by the config file, typically
named mirror_images_config. The config file uses python syntax to define
two lists of image names: images_to_mirror and images_not_to_mirror. Images in
images_to_mirror will be mirrored by this tool. To ensure every image has been
considered for mirroring, the remaining images must be listed in
images_not_to_mirror.

Mirrorable images include directional images (e.g. back and forward buttons) and
asymmetric UI elements (e.g. a left-to-right fade image). Non-mirrorable images
include images with text (e.g. the Chrome logo), symmetric images (e.g. a star
or X button), and images whose asymmetry is not critical to their meaning. Most
images do not need to be mirrored.

Example mirror_images_config:

  images_to_mirror = ['back.png', 'forward.png', 'left_to_right_fade.png']
  images_not_to_mirror = ['star.png', 'chrome_logo.png', 'blank_page.png']

Source images are taken from input_dir/res/drawable-* folders, and the
generated images are saved into output_dir/res/drawable-ldrtl-* folders. For
example: input_dir/res/drawable-hdpi/back.png would be mirrored into
output_dir/res/drawable-ldrtl-hdpi/back.png.
"""

import errno
import multiprocessing.pool
import optparse
import os
import subprocess
import sys

from util import build_utils

BUILD_ANDROID_DIR = os.path.join(os.path.dirname(__file__), '..')
sys.path.append(BUILD_ANDROID_DIR)

from pylib import constants


class Image(object):
  """Represents an image in the Android res directory."""

  def __init__(self, drawable_dir, name):
    # The image's directory, e.g. drawable-hdpi
    self.drawable_dir = drawable_dir
    # The image's filename, e.g. star.png
    self.name = name


class ConfigError(Exception):
  """Error raised when images can't be mirrored because of problems with the
  config file."""

  def __init__(self, error_messages):
    # A list of strings describing the errors in the config file.
    self.error_messages = error_messages


class Project(object):
  """This class knows how to read the config file and mirror images in an
  Android project."""

  def __init__(self, config_file, input_res_dir, output_res_dir):
    """Args:
      config_file: The config file specifying which images will be mirrored.
      input_res_dir: The directory containing source images to be mirrored.
      output_res_dir: The directory into which mirrored images can be saved.
    """
    self.config_file = config_file
    self.input_res_dir = input_res_dir
    self.output_res_dir = output_res_dir

    # List of names of images that will be mirrored, from config file.
    self.images_to_mirror = None
    # List of names of images that will not be mirrored, from config file.
    self.images_not_to_mirror = None
    # List of all images found in res/drawable* directories.
    self.images = None

  def mirror_images(self):
    """Mirrors images in the project according to the configuration.

    If the project configuration contains any errors, this will raise a
    ConfigError exception containing a list of errors that must be addressed
    manually before any images can be mirrored.
    """
    self._read_config_file()
    self._read_drawable_dirs()
    self._verify_config()
    self._mirror_images()

  def _read_config_file(self):
    """Reads the lists of images that should and should not be mirrored from the
    config file.
    """
    exec_env = {}
    execfile(self.config_file, exec_env)
    self.images_to_mirror = exec_env.get('images_to_mirror')
    self.images_not_to_mirror = exec_env.get('images_not_to_mirror')
    errors = []
    self._verify_config_list_well_formed(self.images_to_mirror,
        'images_to_mirror', errors)
    self._verify_config_list_well_formed(self.images_not_to_mirror,
        'images_not_to_mirror', errors)
    if errors:
      raise ConfigError(errors)

  def _verify_config_list_well_formed(self, config_list, list_name, errors):
    """Checks that config_list is a list of strings. If not, appends error
    messages to errors."""
    if type(config_list) != list:
      errors.append('The config file must contain a list named ' + list_name)
    else:
      for item in config_list:
        if not isinstance(item, basestring):
          errors.append('List {0} contains a non-string item: {1}'
              .format(list_name, item))

  def _read_drawable_dirs(self):
    """Gets the list of images in the input drawable directories."""
    self.images = []

    for dir_name in os.listdir(self.input_res_dir):
      dir_components = dir_name.split('-')
      if dir_components[0] != 'drawable' or 'ldrtl' in dir_components[1:]:
        continue
      dir_path = os.path.join(self.input_res_dir, dir_name)
      if not os.path.isdir(dir_path):
        continue

      for image_name in os.listdir(dir_path):
        if image_name.endswith('.png'):
          self.images.append(Image(dir_name, image_name))

  def _verify_config(self):
    """Checks the config file for errors. Raises a ConfigError if errors are
    found."""
    errors = []

    # Ensure images_to_mirror and images_not_to_mirror are sorted with no
    # duplicates.
    for l in self.images_to_mirror, self.images_not_to_mirror:
      for i in range(len(l) - 1):
        if l[i + 1] == l[i]:
          errors.append(l[i + 1] + ' is listed multiple times')
        elif l[i + 1] < l[i]:
          errors.append(l[i + 1] + ' is not in sorted order')

    # Ensure no overlap between images_to_mirror and images_not_to_mirror.
    overlap = set(self.images_to_mirror).intersection(self.images_not_to_mirror)
    for item in sorted(overlap):
      errors.append(item + ' appears in both lists.')

    # Ensure all images in res/drawable* folders are listed in config file.
    images_in_config = set(self.images_to_mirror + self.images_not_to_mirror)
    images_in_res_dir = [i.name for i in self.images]
    images_missing_from_config = set(images_in_res_dir).difference(
        images_in_config)
    for image_name in sorted(images_missing_from_config):
      errors.append(image_name + ' exists in a res/drawable* folder but is not '
          'listed in the config file. Update the config file to specify '
          'whether this image should be mirrored for right-to-left layouts (or '
          'delete the image).')

    # Ensure only images in res/drawable* folders are listed in config file.
    images_not_in_res_dir = set(images_in_config).difference(
        images_in_res_dir)
    for image_name in sorted(images_not_in_res_dir):
      errors.append(image_name + ' is listed in the config file, but does not '
          'exist in any res/drawable* folders. Remove this image name from the '
          'config file (or add the image to a drawable folder).')

    if errors:
      raise ConfigError(errors)

  def _mirror_image(self, image):
    ltr_path = os.path.join(self.input_res_dir, image.drawable_dir, image.name)
    rtl_path = os.path.join(self.output_res_dir,
                            get_rtl_dir(image.drawable_dir), image.name)
    build_utils.MakeDirectory(os.path.dirname(rtl_path))
    mirror_image(ltr_path, rtl_path)

  def _mirror_images(self):
    pool = multiprocessing.pool.ThreadPool()
    images_to_mirror = [i for i in self.images if
                        i.name in self.images_to_mirror]
    pool.map(self._mirror_image, images_to_mirror)


def get_rtl_dir(drawable_dir_ltr):
  """Returns the RTL drawable directory corresponding to drawable_dir_ltr.

  Example:
    drawable-hdpi -> drawable-ldrtl-hdpi
  """
  dir_components = drawable_dir_ltr.split('-')
  assert 'ldrtl' not in dir_components
  # ldrtl is always the first qualifier, as long as mobile country code or
  # language and region aren't used as qualifiers:
  # http://developer.android.com/guide/topics/resources/providing-resources.html
  dir_components.insert(1, 'ldrtl')
  return '-'.join(dir_components)


def mirror_image(src_path, dst_path):
  """Mirrors a single image horizontally. 9-patches are treated specially: the
  left and right edges of a 9-patch are not mirrored.

  Args:
    src_path: The image to be mirrored.
    dst_path: The path where the mirrored image will be saved.
  """
  try:
    if src_path.endswith('.9.png'):
      build_utils.CheckOutput(['convert', src_path,
                               '(', '+clone', '-flop', '-shave', '1x0', ')',
                               '-compose', 'Copy',
                               '-geometry', '+1+0',
                               '-composite', dst_path])
    else:
      build_utils.CheckOutput(['convert', '-flop', src_path, dst_path])
  except OSError as e:
    if e.errno == errno.ENOENT:
      raise Exception('Executable "convert" (from the imagemagick package) not '
          'found. Run build/install-build-deps-android.sh and ensure '
          'that "convert" is on your path.')
    raise


def parse_args(args=None):
  parser = optparse.OptionParser()
  parser.add_option('--config-file', help='Configuration file specifying which '
      'images should be mirrored')
  parser.add_option('--input-res-dir', help='The res folder containing the '
      'source images.')
  parser.add_option('--output-res-dir', help='The res folder into which '
      'mirrored images will be saved.')

  if args is None:
    args = sys.argv[1:]
  options, args = parser.parse_args(args)

  # Check that required options have been provided.
  required_options = ('config_file', 'input_res_dir', 'output_res_dir')
  build_utils.CheckOptions(options, parser, required=required_options)

  return options


def main(args=None):
  options = parse_args(args)
  project = Project(options.config_file, options.input_res_dir,
                    options.output_res_dir)
  try:
    project.mirror_images()
  except ConfigError as e:
    sys.stderr.write('\nFailed to mirror images for RTL layouts.\n')
    sys.stderr.write('{0} error(s) in config file {1}:\n'.format(
        len(e.error_messages),
        os.path.relpath(options.config_file, constants.DIR_SOURCE_ROOT)))
    for error_message in e.error_messages:
      sys.stderr.write(' - {0}\n'.format(error_message))
    sys.stderr.write('For more information on mirroring images, read the '
        'header comment in build/android/gyp/mirror_images.py.\n')
    sys.exit(1)


if __name__ == '__main__':
  main()
