// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pong_model.h"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <limits>

namespace {

/** Width of the ball in pixels. */
const int kBallWidth = 20;
/** Height of the ball in pixels. */
const int kBallHeight = 20;
/** Width of each paddle in pixels. */
const int kPaddleWidth = 10;
/** Height of each paddle in pixels. */
const int kPaddleHeight = 99;
/** Starting X position of the ball, in the range [0, 1]. */
const float kBallStartX = 0.5f;
/** Starting Y position of the ball, in the range [0, 1]. */
const float kBallStartY = 0.9f;
/** Starting X position of the left paddle, in the range [0, 1]. */
const float kLeftPaddleStartX = 0.2f;
/** Starting Y position of the left paddle, in the range [0, 1]. */
const float kLeftPaddleStartY = 0.5f;
/** Starting X position of the right paddle, in the range [0, 1]. */
const float kRightPaddleStartX = 0.8f;
/** Starting Y position of the right paddle, in the range [0, 1]. */
const float kRightPaddleStartY = 0.5f;
/** The number of pixels the ball moves along each axis, per update, by
 * default. */
const int kBallUpdateDistance = 4;
/** The number of pixels the paddle moves per update. */
const int kPaddleUpdateDistance = 4;
/** If the ball hits the paddle in the range [0, kPaddleSpinUp), then the ball
 * will bounce up (even if it was moving down). */
const float kPaddleSpinUp = 0.2f;
/** If the ball hits the paddle in the range (kPaddleSpinDown, 1], then the ball
 * will bounce down (even if it was moving up). */
const float kPaddleSpinDown = 0.8f;

/**
 * Gets the position of an object in a given range.
 * This functions ensures that if fraction is in the range [0, 1], then the
 * return value will be in the range [0, region_max - object_width].
 *
 * fraction == 0.0: return => region_min
 * fraction == 0.5: return => (region_min + region_max - object_width) / 2
 * fraction == 1.0: return => region_max - object_width
 *
 * @param[in] region_min The minimum value of the range.
 * @param[in] region_max The maximum value of the range.
 * @param[in] object_width The width of the object.
 * @param[in] fraction A value in the range [0, 1].
 * @return The left position of the object, in the range [0, region_max -
 *     object_width].
 */
int GetFractionalPos(int region_min,
                     int region_max,
                     int object_width,
                     float fraction) {
  return region_min +
         static_cast<int>((region_max - object_width - region_min) * fraction);
}

/**
 * Set the position of a pp::Rect (i.e. ball or paddle) given the court size.
 *
 * @param[in] court The bounds to place |object| in.
 * @param[in,out] object The object to place in |court|.
 * @param[in] x_fraction A value in the range [0, 1]. 0 is on the left, 1 is on
 *     the right of the court.
 * @param[in] y_fraction A value in the range [0, 1]. 0 is on the top, 1 is on
 *     the bottom of the court.
 */
void SetFractionalPosition(const pp::Rect& court,
                           pp::Rect* object,
                           float x_fraction,
                           float y_fraction) {
  object->set_x(
      GetFractionalPos(court.x(), court.right(), object->width(), x_fraction));
  object->set_y(GetFractionalPos(
      court.y(), court.bottom(), object->height(), y_fraction));
}

}  // namespace

PaddleModel::PaddleModel() : rect(kPaddleWidth, kPaddleHeight) {}

void PaddleModel::SetPosition(const pp::Rect& court,
                              float x_fraction,
                              float y_fraction) {
  SetFractionalPosition(court, &rect, x_fraction, y_fraction);
}

void PaddleModel::Move(const pp::Rect& court,
                       const BallModel& ball,
                       MoveDirection dir) {
  if (dir == MOVE_NONE)
    return;

  int dy = dir == MOVE_UP ? -kPaddleUpdateDistance : kPaddleUpdateDistance;
  rect.Offset(0, dy);

  // Don't allow the paddle to move on top of the ball.
  if (rect.Intersects(ball.rect)) {
    rect.Offset(0, -dy);
    return;
  }

  if (rect.y() < court.y()) {
    rect.set_y(court.y());
  } else if (rect.bottom() > court.bottom()) {
    rect.set_y(court.bottom() - rect.height());
  }
}

PaddleCollision PaddleModel::GetPaddleCollision(float collision_y) const {
  float paddle_relative_y_fraction = (collision_y - rect.y()) / rect.height();
  if (paddle_relative_y_fraction < kPaddleSpinUp)
    return PADDLE_COLLISION_SPIN_UP;
  else if (paddle_relative_y_fraction > kPaddleSpinDown)
    return PADDLE_COLLISION_SPIN_DOWN;
  else
    return PADDLE_COLLISION_NORMAL;
}

BallModel::BallModel()
    : rect(kBallWidth, kBallHeight),
      dx(kBallUpdateDistance),
      dy(kBallUpdateDistance) {}

void BallModel::SetPosition(const pp::Rect& court,
                            float x_fraction,
                            float y_fraction) {
  SetFractionalPosition(court, &rect, x_fraction, y_fraction);
}

void BallModel::Move(float dt) {
  rect.Offset(static_cast<int>(dt * dx), static_cast<int>(dt * dy));
}

float BallModel::GetPaddleCollisionTime(const PaddleModel& paddle) const {
  assert(!rect.Intersects(paddle.rect));

  // Calculate the range of time when the ball will be colliding with the x
  // range of the paddle.
  float float_dx = static_cast<float>(dx);
  float x_collision_t_min = (paddle.rect.right() - rect.x()) / float_dx;
  float x_collision_t_max = (paddle.rect.x() - rect.right()) / float_dx;
  if (x_collision_t_min > x_collision_t_max)
    std::swap(x_collision_t_min, x_collision_t_max);

  // If the last point in time that the ball is colliding, along the x axis, is
  // negative, then the ball would have to move backward to collide with the
  // paddle.
  if (x_collision_t_max <= 0)
    return std::numeric_limits<float>::max();

  // Calculate the range of time when the ball will be colliding with the y
  // range of the paddle.
  float float_dy = static_cast<float>(dy);
  float y_collision_t_min = (paddle.rect.bottom() - rect.y()) / float_dy;
  float y_collision_t_max = (paddle.rect.y() - rect.bottom()) / float_dy;
  if (y_collision_t_min > y_collision_t_max)
    std::swap(y_collision_t_min, y_collision_t_max);

  // See above for the early out logic.
  if (y_collision_t_max <= 0)
    return std::numeric_limits<float>::max();

  // The ball will be colliding with the paddle only when the x range and the y
  // ranges intersect. The first time this happens is the whichever happens
  // later: entering the x range or entering the y range.
  // However, if either range's maximum is less than this value, then the
  // ranges are disjoint, and the ball does not collide with the paddle (it
  // will pass by it instead).
  float max_of_mins = std::max(x_collision_t_min, y_collision_t_min);
  if (x_collision_t_max > max_of_mins && y_collision_t_max > max_of_mins)
    return max_of_mins;

  return std::numeric_limits<float>::max();
}

float BallModel::GetCourtCollisionTime(const pp::Rect& court) const {
  assert(rect.y() >= court.y());
  assert(rect.bottom() <= court.bottom());

  // When the ball is moving up, only check for collision with the top of the
  // court. Likewise, if the ball is moving down only check for collision with
  // the bottom of the court.
  float y_collision_t =
      dy < 0 ? (court.y() - rect.y()) / static_cast<float>(dy)
             : (court.bottom() - rect.bottom()) / static_cast<float>(dy);

  return y_collision_t;
}

void BallModel::ApplyPaddleCollision(PaddleCollision collision) {
  switch (collision) {
    case PADDLE_COLLISION_SPIN_UP:
      dy -= kBallUpdateDistance;
      if (dy == 0)
        dy -= kBallUpdateDistance;
      break;
    case PADDLE_COLLISION_SPIN_DOWN:
      dy += kBallUpdateDistance;
      if (dy == 0)
        dy += kBallUpdateDistance;
      break;
    case PADDLE_COLLISION_NORMAL:
    default:
      break;
  }
}

PongModel::PongModel(PongModelDelegate* delegate)
    : delegate_(delegate), left_score_(0), right_score_(0) {}

void PongModel::SetCourtSize(const pp::Size& size) { court_.set_size(size); }

void PongModel::SetScore(int left_score, int right_score) {
  left_score_ = left_score;
  right_score_ = right_score;
  delegate_->OnScoreChanged();
}

void PongModel::ResetPositions() {
  ball_.SetPosition(court_, kBallStartX, kBallStartY);
  left_paddle_.SetPosition(court_, kLeftPaddleStartX, kLeftPaddleStartY);
  right_paddle_.SetPosition(court_, kRightPaddleStartX, kRightPaddleStartY);
  ball_.dx = kBallUpdateDistance;
  ball_.dy = kBallUpdateDistance;
}

void PongModel::Update(MoveDirection left_movement,
                       MoveDirection right_movement) {
  left_paddle_.Move(court_, ball_, left_movement);
  right_paddle_.Move(court_, ball_, right_movement);
  UpdateBall();

  if (ball_.rect.x() < court_.x()) {
    right_score_++;
    delegate_->OnScoreChanged();
    delegate_->OnPlayerScored();
  } else if (ball_.rect.right() > court_.right()) {
    left_score_++;
    delegate_->OnScoreChanged();
    delegate_->OnPlayerScored();
  }
}

void PongModel::UpdateBall() {
  // If the ball's update distance is large enough, it could collide with
  // multiple objects in one update.
  // To detect this, we calculate when the ball will collide with all objects.
  // If any collisions occur in this update, move the ball to this collision
  // point, and repeat with the rest of the time that's left.
  float dt = 1;
  while (dt > 0) {
    float left_paddle_t = ball_.GetPaddleCollisionTime(left_paddle_);
    float right_paddle_t = ball_.GetPaddleCollisionTime(right_paddle_);
    float min_paddle_t = std::min(left_paddle_t, right_paddle_t);
    float court_t = ball_.GetCourtCollisionTime(court_);
    float min_t = std::min(court_t, min_paddle_t);

    if (min_t > dt) {
      // The ball doesn't hit anything this time.
      ball_.Move(dt);
      return;
    }

    // Move the ball up to the point of collision.
    ball_.Move(min_t);
    dt -= min_t;

    if (court_t < min_paddle_t) {
      // Just bounce off the court, flip ball's dy.
      ball_.dy = -ball_.dy;
    } else {
      // Bouncing off the paddle flips the ball's dx, but also can potentially
      // add "spin" when the ball bounces off the top or bottom of the paddle.
      ball_.dx = -ball_.dx;

      float collision_y = ball_.rect.y() + ball_.rect.height() / 2;
      PaddleCollision paddle_collision;
      if (left_paddle_t < right_paddle_t)
        paddle_collision = left_paddle_.GetPaddleCollision(collision_y);
      else
        paddle_collision = right_paddle_.GetPaddleCollision(collision_y);

      ball_.ApplyPaddleCollision(paddle_collision);
    }
  }
}
