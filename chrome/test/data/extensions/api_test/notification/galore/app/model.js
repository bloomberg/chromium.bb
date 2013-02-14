// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var Galore = Galore || {};

Galore.NOTIFICATIONS = [
  {
    name: 'Simple Notifications',
    templateType: 'simple',
    notifications: [
      {
        iconUrl: '$@/images/man1-$%x$%.jpg',
        title: 'Althe Frazon',
        message: 'Lorem ipsum'
      },
      {
        iconUrl: '$@/images/woman1-$%x$%.jpg',
        title: 'Kathrine Doiss',
        message: 'Dolor sit amet'
      },
      {
        iconUrl: '$@/images/woman2-$%x$%.jpg',
        title: 'Jamie Haynig',
        message: 'Consectetur adipisicing elit'
      },
      {
        iconUrl: '$@/images/woman3-$%x$%.jpg',
        title: 'Maricia Rilari',
        message: 'Sed do eiusmod tempor incididunt'
      }
    ]
  },
  {
    name: 'Basic Notifications',
    templateType: 'basic',
    notifications: [
      {
        iconUrl: '$@/images/man1-$%x$%.jpg',
        title: 'Althe Frazon',
        message: 'Lorem ipsum',
        buttons: [
          {title: 'Call', iconUrl: '$@/images/call-16x16.jpg'},
          {title: 'Send Email', iconUrl: '$@/images/send-16x16.jpg'},
        ]
      },
      {
        iconUrl: '$@/images/woman1-$%x$%.jpg',
        title: 'Kathrine Doiss',
        message: 'Dolor sit amet'
      },
      {
        iconUrl: '$@/images/woman2-$%x$%.jpg',
        title: 'Jamie Haynig',
        message: 'Consectetur adipisicing elit'
      },
      {
        iconUrl: '$@/images/woman3-$%x$%.jpg',
        title: 'Maricia Rilari',
        message: 'Sed do eiusmod tempor incididunt'
      },
      {
        iconUrl: '$@/images/fruit1-$%x$%.jpg',
        title: 'Notification #$#: Apples!',
        message: 'Malus domestica'
      },
      {
        iconUrl: '$@/images/fruit2-$%x$%.jpg',
        title: 'Notification #$#: Pears!',
        message: 'Pyrus'
      },
      {
        iconUrl: '$@/images/fruit3-$%x$%.jpg',
        title: 'Notification #$#: Oranges!',
        message: 'Citrus sinensis'
      },
      {
        iconUrl: '$@/images/fruit4-$%x$%.jpg',
        title: 'Notification #$#: Strawberries!',
        message: 'Fragaria ananassa'
      },
      {
        iconUrl: '$@/images/fruit5-$%x$%.jpg',
        title: 'Notification #$#: Raspberries!',
        message: 'Rubus idaeus'
      },
      {
        iconUrl: '$@/images/fruit6-$%x$%.jpg',
        title: 'Notification #$#: Blueberries!',
        message: 'Vaccinium cyanococcus'
      }
    ]
  },
  {
    name: 'Image Notifications',
    templateType: 'image',
    notifications: [
      {
        iconUrl: '$@/images/woman3-$%x$%.jpg',
        title: 'Maricia Rilari',
        message: 'Pictures from Tahoe',
        imageUrl: '$@/images/tahoe-300x225.jpg'
      },
      {
        iconUrl: '$@/images/flower1-$%x$%.jpg',
        title: 'Notification #$#: Daffodils!',
        message: 'Narcissus',
        imageUrl: '$@/images/image1-300x225.jpg',
        buttons: [
          {title: 'Lorem Ipsum Dolor Sit Amet Consectetur Adipisicing'},
          {title: 'Elit Sed Do'},
        ]
      },
      {
        iconUrl: '$@/images/flower2-$%x$%.jpg',
        title: 'Notification #$#: Sunflowers!',
        message: 'Helianthus annuus',
        imageUrl: '$@/images/image2-300x225.jpg'
      },
      {
        iconUrl: '$@/images/flower3-$%x$%.jpg',
        title: 'Notification #$#: Poinsettias!',
        message: 'Euphorbia pulcherrima',
        imageUrl: '$@/images/image3-300x172.jpg'
      },
      {
        iconUrl: '$@/images/flower4-$%x$%.jpg',
        title: 'Notification #$#: Heather!',
        message: 'Calluna vulgaris',
        imageUrl: '$@/images/image4-200x300.jpg'
      }
    ]
  },
  {
    name: 'List Notifications',
    templateType: 'list',
    notifications: [
      {
        iconUrl: '$@/images/inbox-00-$%x$%.png',
        title: '15 new messages',
        message: 'althe.frazon@gmail.com',
        items: [
          {title: 'Althe Frazon', message: 'Lorem ipsum dolor sit amet'},
          {title: 'Kathrine Doiss', message: 'Consectetur adipisicing elit'},
          {title: 'Jamie Haynig', message: 'Sed do eiusmod tempor incididunt'},
          {title: 'Kelly Seiken', message: 'Ut labore et dolore magna aliqua'},
          {title: 'Maricia Rilari', message: 'Ut enim ad minim veniam'}
        ],
        buttons: [
          {title: 'Send Message', iconUrl: '$@/images/send-16x16.png'}
        ]
      },
      {
        iconUrl: '$@/images/inbox-01-$%x$%.png',
        title: '1 new message',
        message: 'althe.frazon@gmail.com',
        items: [
          {title: 'Althe Frazon', message: 'Lorem ipsum dolor sit amet'}
        ]
      },
      {
        iconUrl: '$@/images/inbox-02-$%x$%.png',
        title: '2 new messages',
        message: 'althe.frazon@gmail.com',
        items: [
          {title: 'Althe Frazon', message: 'Lorem ipsum dolor sit amet'},
          {title: 'Kathrine Doiss', message: 'Consectetur adipisicing elit'}
        ]
      },
      {
        iconUrl: '$@/images/inbox-03-$%x$%.png',
        title: '3 new messages',
        message: 'althe.frazon@gmail.com',
        items: [
          {title: 'Althe Frazon', message: 'Lorem ipsum dolor sit amet'},
          {title: 'Kathrine Doiss', message: 'Consectetur adipisicing elit'},
          {title: 'Jamie Haynig', message: 'Sed do eiusmod tempor incididunt'}
        ]
      },
      {
        iconUrl: '$@/images/inbox-05-$%x$%.png',
        title: '5 new messages',
        message: 'althe.frazon@gmail.com',
        items: [
          {title: 'Althe Frazon', message: 'Lorem ipsum dolor sit amet'},
          {title: 'Kathrine Doiss', message: 'Consectetur adipisicing elit'},
          {title: 'Jamie Haynig', message: 'Sed do eiusmod tempor incididunt'},
          {title: 'Kelly Seiken', message: 'Ut labore et dolore magna aliqua'},
          {title: 'Maricia Rilari', message: 'Ut enim ad minim veniam'}
        ]
      },
      {
        iconUrl: '$@/images/inbox-08-$%x$%.png',
        title: '8 new messages',
        message: 'alex.faa.borg@gmail.com',
        items: [
          {title: 'Althe Frazon', message: 'Lorem ipsum dolor sit amet'},
          {title: 'Kathrine Doiss', message: 'Consectetur adipisicing elit'},
          {title: 'Jamie Haynig', message: 'Sed do eiusmod tempor incididunt'},
          {title: 'Kelly Seiken', message: 'Ut labore et dolore magna aliqua'},
          {title: 'Maricia Rilari', message: 'Ut enim ad minim veniam'},
          {title: 'Althe Frazon', message: 'Lorem ipsum dolor sit amet'},
          {title: 'Kathrine Doiss', message: 'Consectetur adipisicing elit'},
          {title: 'Jamie Haynig', message: 'Sed do eiusmod tempor incididunt'}
        ]
      },
      {
        iconUrl: '$@/images/inbox-13-$%x$%.png',
        title: '13 new messages',
        message: 'alex.faa.borg@gmail.com',
        items: [
          {title: 'Althe Frazon', message: 'Lorem ipsum dolor sit amet'},
          {title: 'Kathrine Doiss', message: 'Consectetur adipisicing elit'},
          {title: 'Jamie Haynig', message: 'Sed do eiusmod tempor incididunt'},
          {title: 'Kelly Seiken', message: 'Ut labore et dolore magna aliqua'},
          {title: 'Maricia Rilari', message: 'Ut enim ad minim veniam'},
          {title: 'Althe Frazon', message: 'Lorem ipsum dolor sit amet'},
          {title: 'Kathrine Doiss', message: 'Consectetur adipisicing elit'},
          {title: 'Jamie Haynig', message: 'Sed do eiusmod tempor incididunt'},
          {title: 'Kelly Seiken', message: 'Ut labore et dolore magna aliqua'},
          {title: 'Maricia Rilari', message: 'Ut enim ad minim veniam'},
          {title: 'Althe Frazon', message: 'Lorem ipsum dolor sit amet'},
          {title: 'Kathrine Doiss', message: 'Consectetur adipisicing elit'},
          {title: 'Jamie Haynig', message: 'Sed do eiusmod tempor incididunt'}
        ]
      },
      {
        iconUrl: '$@/images/plant1-$%x$%.jpg',
        title: 'Notification #$#: Yucca!',
        message: 'Asparagaceae agavoideae',
        items: [
          {title: 'One', message: 'Unus'},
          {title: 'Two', message: 'Duo'},
          {title: 'Three', message: 'Tres'}
        ]
      },
      {
        iconUrl: '$@/images/plant2-$%x$%.jpg',
        title: 'Notification #$#: Fern!',
        message: 'Nephrolepis',
        items: [
          {title: 'One', message: 'Unus'},
          {title: 'Two', message: 'Duo'},
          {title: 'Three', message: 'Tres'}
        ]
      },
      {
        iconUrl: '$@/images/plant3-$%x$%.jpg',
        title: 'Notification #$#: Basil!',
        message: 'Ocimum basilicum',
        items: [
          {title: 'One', message: 'Unus'},
          {title: 'Two', message: 'Duo'},
          {title: 'Three', message: 'Tres'}
        ]
      }
    ]
  }
];
