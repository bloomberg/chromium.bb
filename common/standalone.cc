/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Standalone helper functions
// Mimic barebones nacl multimedia interface for standalone
// non-nacl applications.  Mainly used as a debugging aid.
// * Currently standalone is only supported under Linux

#if defined(STANDALONE)

#include <errno.h>
#include <memory.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "./standalone.h"


struct InfoVideo {
  int32_t                 width;
  int32_t                 height;
  int32_t                 format;
  int32_t                 bits_per_pixel;
  int32_t                 bytes_per_pixel;
  int32_t                 rmask;
  int32_t                 gmask;
  int32_t                 bmask;
  SDL_Surface             *screen;
};


struct InfoAudio {
  SDL_AudioSpec           *audio_spec;
  pthread_mutex_t         mutex;
  pthread_cond_t          condvar;
  int32_t                 ready;
  size_t                  size;
  unsigned char           *stream;
  int32_t                 first;
  int32_t                 shutdown;
  SDL_AudioSpec           obtained;
  pthread_t               thread_self;
};


struct InfoMultimedia {
  // do not move begin (initialization dep)
  volatile int32_t        sdl_init_flags;
  volatile int32_t        initialized;
  volatile int32_t        have_video;
  volatile int32_t        have_audio;
  // do not move end
  InfoVideo               video;
  InfoAudio               audio;
};

static InfoMultimedia nacl_multimedia;
static pthread_mutex_t nacl_multimedia_mutex;

static void Fatal(const char *msg) {
  printf("Fatal: %s\n", msg);
  exit(-1);
}

static int Error(const char *msg) {
  printf("Error: %s\n", msg);
  return -1;
}


struct MultimediaModuleInit {
  MultimediaModuleInit();
  ~MultimediaModuleInit();
};

MultimediaModuleInit::MultimediaModuleInit() {
  memset(&nacl_multimedia, 0, sizeof(nacl_multimedia));
  int r = pthread_mutex_init(&nacl_multimedia_mutex, NULL);
  if (0 != r)
    Fatal("MultimediaModuleInit: mutex init failed\n");
}

MultimediaModuleInit::~MultimediaModuleInit() {
  int r = pthread_mutex_destroy(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("~MultimediaModuleInit: mutex destroy failed\n");
  memset(&nacl_multimedia, 0, sizeof(nacl_multimedia));
}

static MultimediaModuleInit multimedia_module;


int nacl_multimedia_init(int subsys) {
  int32_t r;
  int32_t retval = -1;

  r = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_multimedia_init: mutex lock failed");

  // don't allow nested init
  if (0 != nacl_multimedia.initialized)
    Fatal("nacl_multimedia_init: Already initialized!");

  if (0 != nacl_multimedia.have_video)
    Fatal("nacl_multimedia_init: video initialized?");

  if (0 != nacl_multimedia.have_audio)
    Fatal("nacl_multimedia_init: audio initialized?");

  // map nacl a/v to sdl subsystem init
  if (NACL_SUBSYSTEM_VIDEO == (subsys & NACL_SUBSYSTEM_VIDEO)) {
    nacl_multimedia.sdl_init_flags |= SDL_INIT_VIDEO;
  }
  if (NACL_SUBSYSTEM_AUDIO == (subsys & NACL_SUBSYSTEM_AUDIO)) {
    nacl_multimedia.sdl_init_flags |= SDL_INIT_AUDIO;
  }
  if (SDL_Init(nacl_multimedia.sdl_init_flags)) {
    Error("nacl_multimeida_init: SDL_Init failed");
    errno = EIO;
    goto done;
  }

  nacl_multimedia.initialized = 1;
  retval = 0;

 done:
  r = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("multimedia_init: mutex unlock failed");

  return retval;
}


int nacl_multimedia_is_embedded(int *embedded) {
  // standalone is never embedded in browser
  *embedded = 0;
  return 0;
}


int nacl_multimedia_get_embed_size(int *width, int *height) {
  // standalone is never embedded in browser
  // do not modify width, height
  errno = EIO;
  return -1;
}


int nacl_multimedia_shutdown() {
  int32_t r;

  r = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_multimedia_shutdown: mutex lock failed");
  if (0 == nacl_multimedia.initialized)
    Fatal("nacl_multimedia_shutdown: not initialized!");
  if (0 != nacl_multimedia.have_video)
    Fatal("nacl_multimedia_shutdown: video subsystem not shutdown!");
  if (0 != nacl_multimedia.have_audio)
    Fatal("nacl_multimedia_shutdown: audio subsystem not shutdown!");

  SDL_Quit();
  nacl_multimedia.sdl_init_flags = 0;
  nacl_multimedia.initialized = 0;

  r = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_multimedia_shutdown: mutex unlock failed");
  return 0;
}


int nacl_video_init(int                  width,
                    int                  height) {
  int32_t   r;
  int32_t   retval = -1;
  uint32_t  sdl_video_flags = SDL_DOUBLEBUF | SDL_HWSURFACE;

  r = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_video_init: mutex lock failed");

  if (0 == nacl_multimedia.initialized)
    Fatal("nacl_video_init: multimedia not initialized");

  // for now don't allow app to have more than one SDL window or resize
  if (0 != nacl_multimedia.have_video)
    Fatal("nacl_video_init: already initialized!");

  if (SDL_INIT_VIDEO != (nacl_multimedia.sdl_init_flags & SDL_INIT_VIDEO))
    Fatal("nacl_video_init: video not originally requested");

  //  make sure width & height are not insane
  if ((width < kNaClVideoMinWindowSize) ||
      (width > kNaClVideoMaxWindowSize) ||
      (height < kNaClVideoMinWindowSize) ||
      (height > kNaClVideoMaxWindowSize)) {
    Error("nacl_video_init: invalid window size!");
    errno = EINVAL;
    goto done;
  }

  // width and height must also be multiples of 4
  if ((0 != (width & 0x3)) || (0 != (height & 0x3))) {
    Error("nacl_video_init: width & height must be a multiple of 4!");
    errno = EINVAL;
    goto done;
  }

  nacl_multimedia.have_video = 1;
  nacl_multimedia.video.screen = SDL_SetVideoMode(width, height,
                                                  0, sdl_video_flags);
  if (!nacl_multimedia.video.screen) {
    Error("nacl_video_init: SDL_SetVideoMode failed");
    nacl_multimedia.have_video = 0;
    errno = EIO;
    goto done;
  }

  // width, height and format validated in top half
  nacl_multimedia.video.width = width;
  nacl_multimedia.video.height = height;
  nacl_multimedia.video.rmask = 0x00FF0000;
  nacl_multimedia.video.gmask = 0x0000FF00;
  nacl_multimedia.video.bmask = 0x000000FF;
  nacl_multimedia.video.bits_per_pixel = 32;
  nacl_multimedia.video.bytes_per_pixel = 4;

  /* set the window caption */
  /* todo: as parameter to nacl_video_init? */
  SDL_WM_SetCaption("Standalone Application", "Standalone Application");

  retval = 0;
 done:
  r = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_video_init: mutex unlock failed");
  return retval;
}


int nacl_video_shutdown() {
  int32_t   r;
  int32_t   retval = -1;

  r = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_video_shutdown: mutex lock failed");
  if (0 == nacl_multimedia.initialized)
    Fatal("nacl_video_shutdown: multimedia not initialized!");
  if (0 == nacl_multimedia.have_video) {
    Error("nacl_video_shutdown: video already shutdown!");
    errno = EPERM;
    goto done;
  }
  nacl_multimedia.have_video = 0;
  retval = 0;
 done:
  r = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_video_shutdown: mutex unlock failed");
  return retval;
}


int nacl_video_update(const void *data) {
  int32_t       r;
  int32_t       retval = -1;
  SDL_Surface   *image;

  if (0 == nacl_multimedia.have_video)
    Fatal("nacl_video_update: video not initialized");

  image = SDL_CreateRGBSurfaceFrom((unsigned char*) data,
                                   nacl_multimedia.video.width,
                                   nacl_multimedia.video.height,
                                   nacl_multimedia.video.bits_per_pixel,
                                   nacl_multimedia.video.width *
                                     nacl_multimedia.video.bytes_per_pixel,
                                   nacl_multimedia.video.rmask,
                                   nacl_multimedia.video.gmask,
                                   nacl_multimedia.video.bmask, 0);
  if (NULL == image) {
    Error("nacl_video_update: SDL_CreateRGBSurfaceFrom failed");
    errno = EPERM;
    goto done;
  }

  r = SDL_SetAlpha(image, 0, 255);
  if (0 != r) {
    Error("nacl_video_update SDL_SetAlpha failed");
    errno = EPERM;
    goto done_free_image;
  }

  r = SDL_BlitSurface(image, NULL, nacl_multimedia.video.screen, NULL);
  if (0 != r) {
    Error("nacl_video_update: SDL_BlitSurface failed");
    errno = EPERM;
    goto done_free_image;
  }

  r = SDL_Flip(nacl_multimedia.video.screen);
  if (0 != r) {
    Error("nacl_video_update: SDL_Flip failed");
    errno = EPERM;
    goto done_free_image;
  }

  retval = 0;
  // fall through to free image and closure

 done_free_image:
  SDL_FreeSurface(image);
 done:
  return retval;
}


int nacl_video_poll_event(union NaClMultimediaEvent *event) {
  int retval;
  int repoll;
  int sdl_r;
  SDL_Event sdl_event;

  if (0 == nacl_multimedia.initialized)
    Fatal("nacl_video_poll_event: multmedia not initialized!\n");
  if (0 == nacl_multimedia.have_video)
    Fatal("nacl_video_poll_event: video not initialized");

  do {
    sdl_r = SDL_PollEvent(&sdl_event);
    repoll = 0;
    if (sdl_r == 0) {
      retval = -1;
      errno = ENODATA;
    } else {
      retval = 0;
      switch (sdl_event.type) {
        case SDL_ACTIVEEVENT:
          event->type = NACL_EVENT_ACTIVE;
          event->active.gain = sdl_event.active.gain;
          event->active.state = sdl_event.active.state;
          break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
          event->type = (SDL_KEYUP == sdl_event.type) ?
                         NACL_EVENT_KEY_UP : NACL_EVENT_KEY_DOWN;
          event->key.which = sdl_event.key.which;
          event->key.state = sdl_event.key.state;
          event->key.keysym.scancode = sdl_event.key.keysym.scancode;
          event->key.keysym.sym = sdl_event.key.keysym.sym;
          event->key.keysym.mod = sdl_event.key.keysym.mod;
          event->key.keysym.unicode = sdl_event.key.keysym.unicode;
          break;
        case SDL_MOUSEMOTION:
          event->type = NACL_EVENT_MOUSE_MOTION;
          event->motion.which = sdl_event.motion.which;
          event->motion.state = sdl_event.motion.state;
          event->motion.x = sdl_event.motion.x;
          event->motion.y = sdl_event.motion.y;
          event->motion.xrel = sdl_event.motion.xrel;
          event->motion.yrel = sdl_event.motion.yrel;
          break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
          event->type = (SDL_MOUSEBUTTONUP == sdl_event.type) ?
                         NACL_EVENT_MOUSE_BUTTON_UP :
                         NACL_EVENT_MOUSE_BUTTON_DOWN;
          event->button.which = sdl_event.button.which;
          event->button.button = sdl_event.button.button;
          event->button.state = sdl_event.button.state;
          event->button.x = sdl_event.button.x;
          event->button.y = sdl_event.button.y;
          break;
        case SDL_QUIT:
          event->type = NACL_EVENT_QUIT;
          break;
        default:
          // an sdl event happened, but we don't support it
          // so move along and repoll for another event
          repoll = 1;
          break;
      }
    }
  } while (0 != repoll);

  return retval;
}


void __NaCl_InternalAudioCallback(void *unused, Uint8 *stream, int size) {
  int r;

  r = pthread_mutex_lock(&nacl_multimedia.audio.mutex);
  if (0 != r)
    Fatal("__NaCl_InternalAudioCallback: mutex lock failed");
  if (0 == nacl_multimedia.audio.ready) {
    nacl_multimedia.audio.stream = stream;
    nacl_multimedia.audio.size = size;
    nacl_multimedia.audio.ready = 1;
    r = pthread_cond_signal(&nacl_multimedia.audio.condvar);
    if (0 != r)
      Fatal("__NaCl_InternalAudioCallback: cond var signal failed");
    // wait for return signal
    while ((1 == nacl_multimedia.audio.ready) &&
           (0 == nacl_multimedia.audio.shutdown)) {
      r = pthread_cond_wait(&nacl_multimedia.audio.condvar,
                            &nacl_multimedia.audio.mutex);
      if (0 != r)
        Fatal("__NaCl_InternalAudioCallback: cond var wait failed");
    }
  }
  r = pthread_mutex_unlock(&nacl_multimedia.audio.mutex);
  if (0 != r)
    Fatal("__NaCl_InternalAudioCallback: mutex unlock failed");
}


int nacl_audio_init(enum NaClAudioFormat format,
                    int                  desired_samples,
                    int                  *obtained_samples) {
  SDL_AudioSpec desired;
  int32_t retval = -1;
  int r;

  r = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_audio_init: mutex lock failed");

  if (0 == nacl_multimedia.initialized)
    Fatal("nacl_audio_init: multimedia not initialized!");

  // for now, don't allow an app to open more than one SDL audio device
  if (0 != nacl_multimedia.have_audio)
    Fatal("nacl_audio_init: already initialized!");

  if ((desired_samples < 128) || (desired_samples > 8192)) {
    Error("nacl_audio_init: desired sample value out of range");
    errno = ERANGE;
    goto done;
  }

  memset(&nacl_multimedia.audio, 0, sizeof(nacl_multimedia.audio));
  memset(&desired, 0, sizeof(desired));

  r = pthread_mutex_init(&nacl_multimedia.audio.mutex, NULL);
  if (0 != r)
    Fatal("nacl_audio_init: mutex init failed");
  r = pthread_cond_init(&nacl_multimedia.audio.condvar, NULL);
  if (0 != r)
    Fatal("nacl_audio_init: cond var ctor failed");

  nacl_multimedia.audio.thread_self = 0;
  nacl_multimedia.audio.size = 0;
  nacl_multimedia.audio.ready = 0;
  nacl_multimedia.audio.first = 1;
  nacl_multimedia.audio.shutdown = 0;

  desired.format = AUDIO_S16LSB;
  desired.channels = 2;
  desired.samples = desired_samples;
  desired.callback = __NaCl_InternalAudioCallback;

  if (NACL_AUDIO_FORMAT_STEREO_44K == format) {
    desired.freq = 44100;
  } else if (NACL_AUDIO_FORMAT_STEREO_48K == format) {
    desired.freq = 48000;
  } else {
    // we only support two simple high quality stereo formats
    Error("nacl_audio_init: unsupported format");
    errno = EINVAL;
    goto done;
  }

  if (SDL_OpenAudio(&desired, &nacl_multimedia.audio.obtained) < 0) {
    Error("nacl_audio_init: Couldn't open SDL audio");
    Error(SDL_GetError());
    errno = EIO;
    goto done;
  }

  if (nacl_multimedia.audio.obtained.format != desired.format) {
    Error("nacl_audio_init: Couldn't get desired format");
    errno = EIO;
    goto done_close;
  }

  *obtained_samples = nacl_multimedia.audio.obtained.samples;

  nacl_multimedia.have_audio = 1;
  retval = 0;
  goto done;

 done_close:
  SDL_CloseAudio();

 done:
  r = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_audio_init: mutex unlock failed");

  return retval;
}


int nacl_audio_shutdown() {
  int r;

  r = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_audio_shutdown: mutex lock failed");
  if (0 == nacl_multimedia.initialized)
    Fatal("nacl_audio_shutdown: multimedia not initialized");
  if (0 == nacl_multimedia.have_audio)
    Fatal("nacl_audio_shutdown: audio not initialized!");
  // tell audio thread we're shutting down
  r = pthread_mutex_lock(&nacl_multimedia.audio.mutex);
  if (0 != r)
    Fatal("nacl_audio_shutdown: mutex lock failed");
  SDL_PauseAudio(1);
  nacl_multimedia.audio.shutdown = 1;
  r = pthread_cond_broadcast(&nacl_multimedia.audio.condvar);
  if (0 != r)
    Fatal("nacl_audio_shutdown: cond var broadcast failed");
  r = pthread_mutex_unlock(&nacl_multimedia.audio.mutex);
  if (0 != r)
    Fatal("nacl_audio_shutdown: mutex unlock failed");
  // close out audio
  SDL_CloseAudio();
  // no more callbacks at this point
  nacl_multimedia.have_audio = 0;
  r = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_audio_shutdown: mutex unlock failed");
  return 0;
}



// returns 0 during normal operation.
// returns -1 indicating that it is time to exit the audio thread
int32_t nacl_audio_stream(const void *data,
                          size_t     *size) {
  int32_t r;
  int32_t retval = -1;

  r = pthread_mutex_lock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_audio_stream: global mutex lock failed");

  if (0 == nacl_multimedia.have_audio) {
    goto done;
  }

  if (nacl_multimedia.audio.first) {
    // don't copy data on first call...
    nacl_multimedia.audio.thread_self = pthread_self();
  } else {
    if (nacl_multimedia.audio.thread_self != pthread_self()) {
      Fatal("nacl_audio_stream: called from different thread");
    }

    // copy the audio data into the sdl audio buffer
    memcpy(nacl_multimedia.audio.stream, data, nacl_multimedia.audio.size);
  }

  // callback synchronization
  r = pthread_mutex_lock(&nacl_multimedia.audio.mutex);
  if (0 != r)
    Fatal("nacl_audio_stream: audio mutex lock failed");
  // unpause audio on first, start callbacks
  if (nacl_multimedia.audio.first) {
    SDL_PauseAudio(0);
    nacl_multimedia.audio.first = 0;
  }

  // signal callback (if it is waiting)
  if (nacl_multimedia.audio.ready != 0) {
    nacl_multimedia.audio.ready = 0;
    r = pthread_cond_signal(&nacl_multimedia.audio.condvar);
    if (0 != r)
      Fatal("nacl_audio_stream: cond var signal failed");
  }

  nacl_multimedia.audio.size = 0;

  // wait for next callback
  while ((0 == nacl_multimedia.audio.ready) &&
         (0 == nacl_multimedia.audio.shutdown)) {
    r = pthread_cond_wait(&nacl_multimedia.audio.condvar,
                          &nacl_multimedia.audio.mutex);
    if (0 != r)
      Fatal("nacl_audio_stream: cond var wait failed");
  }

  if (0 != nacl_multimedia.audio.shutdown) {
    nacl_multimedia.audio.size = 0;
    *size = 0;
    goto done;
  }
  // return size of next audio block
  *size = (size_t)nacl_multimedia.audio.size;
  r = pthread_mutex_unlock(&nacl_multimedia.audio.mutex);
  if (0 != r)
    Fatal("nacl_audio_stream: audio mutex unlock failed");
  retval = 0;

 done:
  r = pthread_mutex_unlock(&nacl_multimedia_mutex);
  if (0 != r)
    Fatal("nacl_audio_stream: global mutex unlock failed");
  return retval;
}


#endif  // STANDALONE
