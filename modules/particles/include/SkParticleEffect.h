/*
* Copyright 2019 Google LLC
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
*/

#ifndef SkParticleEffect_DEFINED
#define SkParticleEffect_DEFINED

#include "SkAutoMalloc.h"
#include "SkCurve.h"
#include "SkRandom.h"
#include "SkRefCnt.h"
#include "SkTArray.h"

class SkCanvas;
class SkFieldVisitor;
class SkParticleAffector;
class SkParticleDrawable;
struct SkParticleState;

class SkParticleEffectParams : public SkRefCnt {
public:
    int       fMaxCount = 128;
    float     fEffectDuration = 1.0f;
    float     fRate = 8.0f;
    SkCurve   fLifetime = 1.0f;

    // Drawable (image, sprite sheet, etc.)
    sk_sp<SkParticleDrawable> fDrawable;

    // Rules that configure particles at spawn time
    SkTArray<sk_sp<SkParticleAffector>> fSpawnAffectors;

    // Rules that update existing particles over their lifetime
    SkTArray<sk_sp<SkParticleAffector>> fUpdateAffectors;

    void visitFields(SkFieldVisitor* v);
};

class SkParticleEffect : public SkRefCnt {
public:
    SkParticleEffect(sk_sp<SkParticleEffectParams> params, const SkRandom& random);

    void start(double now, bool looping = false);
    void update(double now);
    void draw(SkCanvas* canvas);

    bool isAlive() const { return fSpawnTime >= 0; }
    int getCount() const { return fCount; }

private:
    void setCapacity(int capacity);

    sk_sp<SkParticleEffectParams> fParams;

    SkRandom fRandom;

    bool   fLooping;
    double fSpawnTime;

    int    fCount;
    double fLastTime;
    float  fSpawnRemainder;

    SkAutoTMalloc<SkParticleState> fParticles;
    SkAutoTMalloc<SkRandom>        fStableRandoms;

    // Cached
    int fCapacity;
};

#endif // SkParticleEffect_DEFINED
