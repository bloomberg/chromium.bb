// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_DATA_PROVIDER_H_
#define CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_DATA_PROVIDER_H_

#include <map>
#include <set>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/files/file_path.h"
#include "base/files/file_path_watcher.h"
#include "chrome/browser/media_galleries/fileapi/iapps_data_provider.h"
#include "chrome/browser/media_galleries/fileapi/safe_iapps_library_parser.h"

namespace itunes {

class TestITunesDataProvider;

// This class is the holder for iTunes parsed data. Given a path to the iTunes
// library XML file it will read it in, parse the data, and provide convenient
// methods to access it.  When the file changes, it will update the data.
// It is not thread safe, but can be run on any thread with IO access.
class ITunesDataProvider : public iapps::IAppsDataProvider {
 public:
  typedef std::string ArtistName;
  typedef std::string AlbumName;
  typedef std::string TrackName;
  typedef std::map<TrackName, base::FilePath> Album;

  explicit ITunesDataProvider(const base::FilePath& library_path);
  virtual ~ITunesDataProvider();

  // Get the platform path for the auto-add directory.
  virtual const base::FilePath& auto_add_path() const;

  // Returns true if |artist| exists in the library.
  bool KnownArtist(const ArtistName& artist) const;

  // Returns true if |artist| has an album by the name |album| in the library.
  bool KnownAlbum(const ArtistName& artist, const AlbumName& album) const;

  // Get the track named (filename basename) |track| in |album| by |artist|.
  // If no such track exists, an empty FilePath is returned.
  base::FilePath GetTrackLocation(const ArtistName& artist,
                                  const AlbumName& album,
                                  const TrackName& track) const;

  // Get the set of artists.
  std::set<ArtistName> GetArtistNames() const;

  // Get the set of albums for |artist|.
  std::set<AlbumName> GetAlbumNames(const ArtistName& artist) const;

  // Get the tracks for the |album| by |artist|.
  Album GetAlbum(const ArtistName& artist, const AlbumName& album) const;

 private:
  friend class TestITunesDataProvider;

  typedef std::map<AlbumName, Album> Artist;
  typedef std::map<ArtistName, Artist> Library;

  // Parse the library xml file.
  virtual void DoParseLibrary(const base::FilePath& library_path,
                              const ReadyCallback& ready_callback) OVERRIDE;

  // Called when the utility process finishes parsing the library XML file.
  void OnLibraryParsed(const ReadyCallback& ready_callback,
                       bool result,
                       const parser::Library& library);

  // Path to the auto-add directory.
  const base::FilePath auto_add_path_;

  // The parsed and uniquified data.
  Library library_;

  scoped_refptr<iapps::SafeIAppsLibraryParser> xml_parser_;

  // Hides parent class member, but it is private, and there's no way to get a
  // WeakPtr<Derived> from a WeakPtr<Base> without using SupportsWeakPtr.
  base::WeakPtrFactory<ITunesDataProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ITunesDataProvider);
};

}  // namespace itunes

#endif  // CHROME_BROWSER_MEDIA_GALLERIES_FILEAPI_ITUNES_DATA_PROVIDER_H_
