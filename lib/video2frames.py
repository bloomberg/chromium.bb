"""
Copyright (c) 2014, Google Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.
    * Neither the name of the <ORGANIZATION> nor the names of its contributors
    may be used to endorse or promote products derived from this software
    without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""
import logging
import os
import sys
import subprocess
import re
import math
import glob
from PIL import Image

def process(video, directory, force):
    histogram_file = os.path.join(directory, 'histograms.json')
    if not os.path.isfile(histogram_file) or force:
        if os.path.isfile(video):
            video = os.path.realpath(video)
            logging.info("Processing frames from video " + video + " to " + directory)
            if not os.path.isdir(directory):
                os.mkdir(directory, 0644)
            if os.path.isdir(directory):
                directory = os.path.realpath(directory)
                clean_directory(directory)
                if extract_frames(video, directory):
                    viewport = find_viewport(directory)
                    eliminate_duplicate_frames(directory, viewport)
                    calculate_histograms(directory)
                    convert_to_jpeg(directory)
                else:
                    logging.critical("Error extracting the video frames from " + video)
            else:
                logging.critical("Error creating output directory: " + directory)
        else:
            logging.critical("Input video file " + video + " does not exist")
    else:
        logging.info("Extracted video already exists in " + directory)


def extract_frames(video, directory):
    ok = False
    logging.info("Extracting frames from " + video + " to " + directory)
    command = [get_exe('ffmpeg', 'ffmpeg'), '-v', 'debug', '-i', video, '-vsync',  '0',
               '-vf', 'mpdecimate=hi=0:lo=0:frac=0,scale=iw*min(400/iw\,400/ih):ih*min(400/iw\,400/ih)',
               os.path.join(directory, 'img-%d.png')]
    logging.debug(' '.join(command))
    lines = []
    p = subprocess.Popen(command, stderr=subprocess.PIPE)
    while p.poll() is None:
        lines.extend(iter(p.stderr.readline, ""))

    match = re.compile('keep pts:[0-9]+ pts_time:(?P<timecode>[0-9\.]+)')
    frame_count = 0
    for line in lines:
        m = re.search(match, line)
        if m:
            frame_count += 1
            frame_time = int(math.ceil(float(m.groupdict().get('timecode')) * 1000))
            src = os.path.join(directory, 'img-{0:d}.png'.format(frame_count))
            dest = os.path.join(directory, 'video-{0:06d}.png'.format(frame_time))
            logging.debug('Renaming ' + src + ' to ' + dest)
            os.rename(src, dest)
            ok = True

    return ok


def find_viewport(directory):
    viewport = None
    try:
        frames = sorted(glob.glob(os.path.join(directory, 'video-*.png')))
        frame = frames[0]
        if is_orange_frame(frame):
            # Figure out the viewport
            im = Image.open(frame)
            width, height = im.size
            logging.debug('{0} is {1:d}x{2:d}'.format(frame, width, height))
            x = int(math.floor(width / 2))
            y = int(math.floor(height / 2))
            pixels = im.load()
            orange = pixels[x, y]

            # Find the left edge
            left = None
            while left is None and x >= 0:
                if not colors_are_similar(orange, pixels[x, y]):
                    left = x + 1
                else:
                    x -= 1
            if left is None:
                left = 0
            logging.debug('Viewport left edge is {0:d}'.format(left))

            # Find the right edge
            x = int(math.floor(width / 2))
            right = None
            while right is None and x < width:
                if not colors_are_similar(orange, pixels[x, y]):
                    right = x - 1
                else:
                    x += 1
            if right is None:
                right = width
            logging.debug('Viewport right edge is {0:d}'.format(right))

            # Find the top edge
            x = int(math.floor(width / 2))
            top = None
            while top is None and y >= 0:
                if not colors_are_similar(orange, pixels[x, y]):
                    top = y + 1
                else:
                    y -= 1
            if top is None:
                top = 0
            logging.debug('Viewport top edge is {0:d}'.format(top))

            # Find the bottom edge
            y = int(math.floor(height / 2))
            bottom = None
            while bottom is None and y < height:
                if not colors_are_similar(orange, pixels[x, y]):
                    bottom = y - 1
                else:
                    y +=1
            if bottom is None:
                bottom = height
            logging.debug('Viewport bottom edge is {0:d}'.format(bottom))

            viewport = {'x': left, 'y': top, 'width': (right - left), 'height': (bottom - top)}

            # remove all of the orange frames if there are more than one
            os.remove(frame)
            for frame in frames:
                if os.path.isfile(frame):
                    if is_orange_frame(frame):
                        os.remove(frame)
                    else:
                        break

            #re-number the video frames
            offset = None
            frames = sorted(glob.glob(os.path.join(directory, 'video-*.png')))
            match = re.compile('video-(?P<ms>[0-9]+)\.png')
            for frame in frames:
                m = re.search(match, frame)
                if m is not None:
                    frame_time = int(m.groupdict().get('ms'))
                    if offset is None:
                        offset = frame_time
                    new_time = frame_time - offset
                    dest = os.path.join(directory, 'video-{0:06d}.png'.format(new_time))
                    os.rename(frame, dest)
    except:
        viewport = None

    return viewport


def eliminate_duplicate_frames(directory, viewport):
    return


def calculate_histograms(directory):
    return


def convert_to_jpeg(directory):
    return

def get_exe(package, app):
    bits = 'x86'
    extension = ''
    if sys.maxsize > 2**32:
        bits = 'x64'
    os_name = 'linux'
    if sys.platform.startswith('win'):
        os_name = 'win'
        extension = '.exe'
    if sys.platform.startswith('darwin'):
        os_name = 'osx'
        bits = 'x64'
    exe = package + '/' + os_name + '-' + bits + '/' + app + extension

    return os.path.realpath(exe)


def clean_directory(directory):
    files = glob.glob(os.path.join(directory, '*.png'))
    for file in files:
        os.remove(file)
    files = glob.glob(os.path.join(directory, '*.jpg'))
    for file in files:
        os.remove(file)
    files = glob.glob(os.path.join(directory, '*.json'))
    for file in files:
        os.remove(file)


def is_orange_frame(file):
    orange = False
    command = 'convert  "' + os.path.realpath('images/orange.png') + '" ( "' + file + '"' +\
              ' -gravity Center -crop 80x50%+0+0 -resize 200x200! ) miff:- | compare -metric AE - -fuzz 10% null:'
    compare = subprocess.Popen(command, stderr=subprocess.PIPE, shell=True)
    out, err = compare.communicate()
    if re.match('^[0-9]+$', err):
        different_pixels = int(err)
        if different_pixels < 100:
            orange = True

    return orange


def colors_are_similar(a, b):
    similar = True
    for x in range (0, 3):
        if abs(a[x] - b[x]) > 25:
            similar = False

    return similar