// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Namespace object for file type utility functions.
 */
var FileType = {};

FileType.types = {
  // Images
  'jpeg': {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'JPEG'},
  'jpg':  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'JPEG'},
  'bmp':  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'BMP'},
  'gif':  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'GIF'},
  'ico':  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'ICO'},
  'png':  {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'PNG'},
  'webp': {type: 'image', name: 'IMAGE_FILE_TYPE', subtype: 'WebP'},

  // Video
  '3gp':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: '3GP'},
  'avi':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'AVI'},
  'mov':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'QuickTime'},
  'mp4':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
  'm4v':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
  'mpg':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
  'mpeg': {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
  'mpg4': {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
  'mpeg4': {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'MPEG'},
  'ogm':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'OGG'},
  'ogv':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'OGG'},
  'ogx':  {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'OGG'},
  'webm': {type: 'video', name: 'VIDEO_FILE_TYPE', subtype: 'WebM'},

  // Audio
  'flac': {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'FLAC'},
  'mp3':  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'MP3'},
  'm4a':  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'MPEG'},
  'oga':  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'OGG'},
  'ogg':  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'OGG'},
  'wav':  {type: 'audio', name: 'AUDIO_FILE_TYPE', subtype: 'WAV'},

  // Text
  'pod': {type: 'text', name: 'PLAIN_TEXT_FILE_TYPE', subtype: 'POD'},
  'rst': {type: 'text', name: 'PLAIN_TEXT_FILE_TYPE', subtype: 'RST'},
  'txt': {type: 'text', name: 'PLAIN_TEXT_FILE_TYPE', subtype: 'TXT'},
  'log': {type: 'text', name: 'PLAIN_TEXT_FILE_TYPE', subtype: 'LOG'},

  // Others
  'zip': {type: 'archive', name: 'ZIP_ARCHIVE_FILE_TYPE'},
  'rar': {type: 'archive', name: 'RAR_ARCHIVE_FILE_TYPE'},
  'tar': {type: 'archive', name: 'TAR_ARCHIVE_FILE_TYPE'},
  'tar.bz2': {type: 'archive', name: 'TAR_BZIP2_ARCHIVE_FILE_TYPE'},
  'tbz': {type: 'archive', name: 'TAR_BZIP2_ARCHIVE_FILE_TYPE'},
  'tbz2': {type: 'archive', name: 'TAR_BZIP2_ARCHIVE_FILE_TYPE'},
  'tar.gz': {type: 'archive', name: 'TAR_GZIP_ARCHIVE_FILE_TYPE'},
  'tgz': {type: 'archive', name: 'TAR_GZIP_ARCHIVE_FILE_TYPE'},

  'pdf': {type: 'text', icon: 'pdf', name: 'PDF_DOCUMENT_FILE_TYPE',
          subtype: 'PDF'},
  'html': {type: 'text', icon: 'html', name: 'HTML_DOCUMENT_FILE_TYPE',
           subtype: 'HTML'},
  'htm': {type: 'text', icon: 'html', name: 'HTML_DOCUMENT_FILE_TYPE',
          subtype: 'HTML'}
};

FileType.previewArt = {
  'audio': 'images/filetype_large_audio.png',
  'folder': 'images/filetype_large_folder.png',
  'unknown': 'images/filetype_large_generic.png',
  'image': 'images/filetype_large_image.png',
  'video': 'images/filetype_large_video.png'
};

/**
 * Extract extension from the file name and convert it to lower case.
 *
 * @param {string} url
 * @return {string}
 */
FileType.getFileExtension_ = function (url) {
  var extIndex = url.lastIndexOf('.');
  if (extIndex < 0)
    return '';
  return url.substr(extIndex + 1).toLowerCase();
};

FileType.getType = function(url) {
  var extension = FileType.getFileExtension_(url);
  if (extension in FileType.types)
    return FileType.types[extension];
  return {};
};

/**
 * Get the media type for a given url.
 *
 * @param {string} url
 * @return {string} The value of 'type' property from one of the elements in
 *   FileType.types or undefined.
 */
FileType.getMediaType = function(url) {
  return FileType.getType(url).type;
};

/**
 * Get the preview url for a given type.
 *
 * @param {string} type
 * @return {string}
 */
FileType.getPreviewArt = function(type) {
  return FileType.previewArt[type] || FileType.previewArt['unknown'];
};

FileType.MAX_PREVIEW_PIXEL_COUNT = 1 << 21; // 2 MPix
FileType.MAX_PREVIEW_FILE_SIZE = 1 << 20; // 1 Mb

/**
 * If an image file does not have an embedded thumbnail we might want to use
 * the image itself as a thumbnail. If the image is too large it hurts
 * the performance very much so we allow it only for moderately sized files.
 *
 * @param {Object} metadata
 * @param {number} opt_size The file size to be used if the metadata does not
 *   contain fileSize.
 * @return {boolean} Whether it is OK to use the image url for a preview.
 */
FileType.canUseImageUrlForPreview = function(metadata, opt_size) {
  var fileSize = metadata.fileSize || opt_size;
  return ((fileSize && fileSize <= FileType.MAX_PREVIEW_FILE_SIZE)  ||
      (metadata.width && metadata.height &&
      (metadata.width * metadata.height <= FileType.MAX_PREVIEW_PIXEL_COUNT)));
};
