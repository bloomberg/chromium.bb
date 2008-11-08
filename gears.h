#ifndef _GEARS_H_
#define _GEARS_H_

struct gears;

struct gears *gears_create(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha);

void gears_draw(struct gears *gears, GLfloat angle);

#endif
