// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_NINE_PART_BUTTON_CELL_H_
#define CHROME_BROWSER_UI_COCOA_NINE_PART_BUTTON_CELL_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"

// Handles painting of a button using images from a ResourceBundle. The
// constructor expects an array of 9 image resource ids. The images
// corresponding to the resource ids are assigned to a 3 by 3 grid in row major
// order, i.e.
//  _________________
// |_id0_|_id1_|_id2_|
// |_id3_|_id4_|_id5_|
// |_id6_|_id7_|_id8_|
@interface NinePartButtonCell : NSButtonCell {
 @private
  base::scoped_nsobject<NSMutableArray> images_;
}

// Loads the images from the ResourceBundle using the provided image resource
// ids and sets the image of the cell.
- (id)initWithResourceIds:(const int[9])ids;

@end  // @interface NinePartButtonCell

#endif  // CHROME_BROWSER_UI_COCOA_NINE_PART_BUTTON_CELL_H_
